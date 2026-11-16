#include "backend.h"

#include <QDir>
#include <QDirIterator>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
QString stripFieldCodes(QString exec)
{
    static const QRegularExpression code(R"(%[fFuUdDnNickvm])");
    exec.remove(code);
    exec.replace("  ", " ");
    return exec.trimmed();
}

QString desktopIdFromPath(const QString &root, const QString &path)
{
    QString relative = QDir(root).relativeFilePath(path);
    relative.remove(QRegularExpression("\\.desktop$"));
    relative.replace('/', '-');
    return relative;
}

bool usableClass(const QString &value)
{
    if (value.trimmed().isEmpty() || value != value.trimmed()) return false;
    if (value.contains("@@") || value.contains('%')) return false;
    static const QRegularExpression invalid(R"(\s|[;&|$`<>(){}[\]*?!"'\\])");
    return !invalid.match(value).hasMatch();
}

QString browserPrefix()
{
    QProcess process;
    process.start("xdg-settings", {"get", "default-web-browser"});
    if (!process.waitForStarted(800) || !process.waitForFinished(1500) || process.exitCode() != 0)
        return "chromium";
    QString id = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    id.remove(QRegularExpression("\\.desktop$"));
    if (id.endsWith("-browser")) id.chop(8);
    if (id.isEmpty()) return "chromium";
    return id;
}

QString webAppClass(const QString &exec, const QString &browser)
{
    const auto match = QRegularExpression(R"(omarchy-launch-webapp\s+(\S+))").match(exec);
    if (!match.hasMatch()) return {};
    QString url = match.captured(1);
    url.remove(QRegularExpression("^[a-zA-Z]+://"));
    if (url.endsWith('/')) url.chop(1);
    url.replace('/', '_');
    return browser + "-" + url + "__-Default";
}

QString steamClass(const QString &exec)
{
    const auto match = QRegularExpression(R"(steam://rungameid/(\d+))").match(exec);
    return match.hasMatch() ? "steam_app_" + match.captured(1) : QString();
}

QString tuiClass(const QString &exec)
{
    const auto match = QRegularExpression(R"(--app-id=([^\s"']+))").match(exec);
    if (match.hasMatch()) return match.captured(1);
    if (exec.contains("omarchy-launch-tui")) {
        const auto name = QRegularExpression(R"(omarchy-launch-tui\s+(\S+))").match(exec);
        if (name.hasMatch()) return "org.omarchy." + name.captured(1);
    }
    return {};
}

bool isPlacementOnly(const QJsonObject &rule)
{
    const QJsonObject settings = rule.value("settings").toObject();
    static const QSet<QString> placementKeys{
        "match", "matchTitle", "classMatchMode", "titleMatchMode", "excludeTitle", "layout",
        "workspace", "noInitialFocus", "autostart", "launchCommand", "launchDesktop",
        "startupOnly", "startupWindow", "error"};
    for (auto it = settings.cbegin(); it != settings.cend(); ++it)
        if (!placementKeys.contains(it.key())) return false;
    return true;
}

QString quoted(const QString &value)
{
    if (value.isEmpty()) return "''";
    static const QRegularExpression safe(R"(^[A-Za-z0-9_.:/=@%+-]+$)");
    if (safe.match(value).hasMatch()) return value;
    QString escaped = value;
    escaped.replace("'", "'\\''");
    return "'" + escaped + "'";
}
}

QString Backend::deriveApplicationClass(const QString &desktopId, const QString &startupWMClass,
                                        const QString &exec, const QString &browser)
{
    if (usableClass(startupWMClass)) return startupWMClass;
    if (const auto steam = steamClass(exec); !steam.isEmpty()) return steam;
    if (const auto tui = tuiClass(exec); !tui.isEmpty()) return tui;
    if (const auto web = webAppClass(exec, browser); !web.isEmpty()) return web;
    QString id = desktopId;
    id.remove(QRegularExpression("\\.desktop$"));
    return id;
}

QString Backend::desktopExecCommand(const QString &exec)
{
    return stripFieldCodes(exec);
}

QString Backend::desktopLaunchTarget(const QString &desktopId, const QString &exec)
{
    QString id = desktopId;
    if (!id.isEmpty() && !id.endsWith(".desktop")) id += ".desktop";
    if (!id.isEmpty()) return id;
    return desktopExecCommand(exec);
}

QString Backend::appSpacePath() const
{
    const QString state = qEnvironmentVariable("XDG_STATE_HOME",
        QDir::homePath() + "/.local/state");
    return state + "/omarchy/appspace/rules.json";
}

QString Backend::appSpaceImportPath(const QString &path) const
{
    return path.trimmed().isEmpty() ? appSpacePath() : path.trimmed();
}

QVariantList Backend::applicationCatalog() const
{
    QJsonArray rules;
    QString error;
    readRuleData(&rules, &error);

    struct Entry {
        QString klass, name, desktop, exec, command, iconData;
        bool running = false, inferred = false, verified = false;
    };
    QHash<QString, Entry> entries; // lower-case class
    QSet<QString> desktopSeen;

    const QString browser = browserPrefix();
    for (const QString &root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        QDirIterator it(root, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString id = desktopIdFromPath(root, path);
            if (desktopSeen.contains(id)) continue;
            desktopSeen.insert(id);
            QSettings entry(path, QSettings::IniFormat);
            entry.beginGroup("Desktop Entry");
            if (entry.value("Hidden", false).toBool() || entry.value("NoDisplay", false).toBool() ||
                entry.value("Type", "Application").toString() != "Application") continue;
            const QString exec = entry.value("Exec").toString();
            if (exec.trimmed().isEmpty()) continue;
            const bool flatpak = path.contains("/flatpak/exports/");
            const QString declared = flatpak ? QString() : entry.value("StartupWMClass").toString();
            QString klass = deriveApplicationClass(id, declared, exec, browser);
            if (klass.isEmpty()) continue;
            Entry row;
            row.klass = klass;
            row.name = entry.value("Name", id).toString();
            row.desktop = id;
            row.exec = exec;
            row.command = desktopExecCommand(exec);
            const QString iconName = entry.value("Icon").toString();
            const QIcon icon = QFileInfo(iconName).isAbsolute() ? QIcon(iconName) : QIcon::fromTheme(iconName);
            if (!icon.isNull()) {
                const QPixmap pixmap = icon.pixmap(64, 64);
                QByteArray png;
                QBuffer buffer(&png);
                if (buffer.open(QIODevice::WriteOnly) && pixmap.save(&buffer, "PNG"))
                    row.iconData = "data:image/png;base64," + QString::fromLatin1(png.toBase64());
            }
            row.inferred = !usableClass(declared);
            entries.insert(klass.toLower(), row);
        }
    }

    // A live window always wins: Hyprland compares against its spelling, and a
    // desktop entry is only a hint.
    for (const QVariant &value : m_windows) {
        const QVariantMap window = value.toMap();
        const QString klass = window.value("class").toString();
        if (klass.isEmpty()) continue;
        const QString key = klass.toLower();
        if (!entries.contains(key)) {
            Entry row;
            row.klass = klass;
            row.name = window.value("currentTitle").toString().isEmpty() ? klass : window.value("currentTitle").toString();
            entries.insert(key, row);
        }
        Entry &row = entries[key];
        row.klass = klass;
        row.running = true;
        row.verified = true;
        row.inferred = false;
    }

    for (const QJsonValue &value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.contains("enabled") && !rule.value("enabled").toBool()) continue;
        const QJsonObject match = rule.value("match").toObject();
        QString klass = match.value("class").toString(rule.value("class").toString());
        klass.remove(QRegularExpression("^\\.\\*|\\$\\.\\*$|^\\^|\\$$"));
        if (klass.isEmpty() || !usableClass(klass)) continue;
        const QString key = klass.toLower();
        if (!entries.contains(key)) {
            Entry row;
            row.klass = klass;
            row.name = rule.value("name").toString(klass);
            entries.insert(key, row);
        }
        entries[key].klass = klass;
    }

    QVariantList result;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        const Entry &row = it.value();
        QVariantMap item{{"class", row.klass}, {"name", row.name}, {"desktop", row.desktop},
            {"iconData", row.iconData},
            {"exec", row.exec}, {"command", row.command}, {"running", row.running},
            {"inferred", row.inferred}, {"verified", row.verified}, {"pinned", false},
            {"workspace", ""}, {"silent", false}, {"autostart", false}, {"startupOnly", false},
            {"startupWindow", 20}};
        for (const QJsonValue &value : rules) {
            const QJsonObject rule = value.toObject();
            if (rule.contains("enabled") && !rule.value("enabled").toBool()) continue;
            const QJsonObject match = rule.value("match").toObject();
            const QString ruleClass = match.value("class").toString(rule.value("class").toString());
            if (ruleClass.compare(row.klass, Qt::CaseInsensitive) != 0) continue;
            const QJsonObject settings = rule.value("settings").toObject();
            item["pinned"] = true;
            item["workspace"] = settings.value("workspace").toString();
            item["silent"] = settings.value("noInitialFocus").toBool();
            item["autostart"] = settings.value("autostart").toBool();
            item["startupOnly"] = settings.value("startupOnly").toBool();
            item["startupWindow"] = settings.value("startupWindow").toInt(20);
            if (!rule.value("name").toString().isEmpty()) item["name"] = rule.value("name").toString();
            break;
        }
        result.append(item);
    }
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        const auto left = a.toMap(), right = b.toMap();
        if (left.value("running").toBool() != right.value("running").toBool())
            return left.value("running").toBool();
        if (left.value("pinned").toBool() != right.value("pinned").toBool())
            return left.value("pinned").toBool();
        return left.value("name").toString().localeAwareCompare(right.value("name").toString()) < 0;
    });
    return result;
}

QVariantMap Backend::applicationSettings(const QString &klass) const
{
    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) return {};
    for (const QJsonValue &value : rules) {
        const QJsonObject rule = value.toObject();
        const QJsonObject match = rule.value("match").toObject();
        const QString ruleClass = match.value("class").toString(rule.value("class").toString());
        if (ruleClass.compare(klass, Qt::CaseInsensitive) == 0)
            return rule.value("settings").toObject().toVariantMap();
    }
    return {};
}

bool Backend::setApplicationPlacement(const QString &klass, const QVariantMap &placement)
{
    if (!usableClass(klass)) { setStatus(tr("That application class is not usable as a match.")); return false; }
    const QString workspace = placement.value("workspace").toString().trimmed();
    if (!workspace.isEmpty() && !QRegularExpression("^([1-9]|10)$").match(workspace).hasMatch()) {
        setStatus(tr("Choose a workspace from 1 to 10, or Any."));
        return false;
    }
    const int startupWindow = qBound(1, placement.value("startupWindow", 20).toInt(), 604800);
    const bool silent = placement.value("silent", false).toBool();
    const bool autostart = placement.value("autostart", false).toBool();
    const bool startupOnly = placement.value("startupOnly", false).toBool() && !workspace.isEmpty();
    const QString launchCommand = placement.value("launchCommand").toString();
    const QString launchDesktop = placement.value("launchDesktop").toString();
    if (autostart && launchCommand.trimmed().isEmpty() && launchDesktop.trimmed().isEmpty()) {
        setStatus(tr("Launch at login needs a desktop entry or command."));
        return false;
    }

    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }

    int target = -1;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        const QJsonObject match = rule.value("match").toObject();
        const QString ruleClass = match.value("class").toString(rule.value("class").toString());
        if (ruleClass.compare(klass, Qt::CaseInsensitive) != 0) continue;
        if (!match.value("title").toString().isEmpty()) continue;
        target = i;
        break;
    }

    const bool empty = workspace.isEmpty() && !silent && !autostart;
    if (target >= 0) {
        QJsonObject rule = rules.at(target).toObject();
        QJsonObject settings = rule.value("settings").toObject();
        if (empty && isPlacementOnly(rule)) {
            rules.removeAt(target);
            if (!commitRuleData(rules)) return false;
            setStatus(tr("Placement removed for %1.").arg(klass));
            emit catalogChanged();
            return true;
        }
        settings.insert("workspace", workspace);
        settings.insert("noInitialFocus", silent);
        settings.insert("autostart", autostart);
        settings.insert("launchCommand", launchCommand);
        settings.insert("launchDesktop", launchDesktop);
        settings.insert("startupOnly", startupOnly);
        settings.insert("startupWindow", startupWindow);
        if (!settings.contains("layout")) settings.insert("layout", "keep");
        if (!settings.contains("match")) settings.insert("match", rule.value("match").toObject());
        if (!settings.contains("classMatchMode")) settings.insert("classMatchMode", "exact");
        if (!settings.contains("titleMatchMode")) settings.insert("titleMatchMode", "exact");
        rule.insert("settings", settings);
        rules.replace(target, rule);
    } else if (!empty) {
        QJsonObject match;
        match.insert("class", klass);
        QJsonObject settings;
        settings.insert("match", match);
        settings.insert("matchTitle", false);
        settings.insert("classMatchMode", "exact");
        settings.insert("titleMatchMode", "exact");
        settings.insert("layout", "keep");
        settings.insert("workspace", workspace);
        settings.insert("noInitialFocus", silent);
        settings.insert("autostart", autostart);
        settings.insert("launchCommand", launchCommand);
        settings.insert("launchDesktop", launchDesktop);
        settings.insert("startupOnly", startupOnly);
        settings.insert("startupWindow", startupWindow);
        QJsonObject rule;
        rule.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
        rule.insert("class", klass);
        rule.insert("title", "");
        rule.insert("name", placement.value("name", klass).toString());
        rule.insert("match", match);
        rule.insert("settings", settings);
        rule.insert("enabled", true);
        rule.insert("appOrigin", "appspace");
        rules.append(rule);
    } else {
        setStatus(tr("Nothing to change for %1.").arg(klass));
        return true;
    }
    if (!commitRuleData(rules)) return false;
    setStatus(tr("Saved placement for %1. Hyprland reloaded.").arg(klass));
    emit catalogChanged();
    return true;
}

bool Backend::clearApplicationPlacement(const QString &klass)
{
    return setApplicationPlacement(klass, {{"workspace", ""}, {"silent", false}, {"autostart", false}});
}

QJsonObject Backend::workspaceData() const
{
    return m_library.value("workspaces").toObject();
}

QJsonObject Backend::monitorData() const
{
    return m_library.value("monitors").toObject();
}

QVariantList Backend::workspacePlan() const
{
    const QJsonObject workspaces = workspaceData();
    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) return {};
    QVariantList result;
    for (int slot = 1; slot <= 10; ++slot) {
        const QString key = QString::number(slot);
        const QJsonObject entry = workspaces.value(key).toObject();
        const QString monitor = entry.value("monitor").toString();
        QStringList apps;
        for (const QJsonValue &value : rules) {
            const QJsonObject rule = value.toObject();
            if (rule.value("settings").toObject().value("workspace").toString() == key)
                apps << rule.value("class").toString();
        }
        result.append(QVariantMap{{"slot", key}, {"monitor", monitor},
            {"persistent", entry.value("persistent").toBool()}, {"apps", apps}});
    }
    return result;
}

bool Backend::setWorkspacePlan(const QString &slot, const QString &monitor, bool persistent)
{
    if (!QRegularExpression("^([1-9]|10)$").match(slot).hasMatch()) return false;
    QJsonObject workspaces = workspaceData();
    QJsonObject entry;
    entry.insert("monitor", monitor);
    entry.insert("persistent", persistent);
    workspaces.insert(slot, entry);
    return commitPlacementData(workspaces, monitorData(),
        tr("Workspace %1 saved.").arg(slot));
}

bool Backend::removeWorkspacePlan(const QString &slot)
{
    QJsonObject workspaces = workspaceData();
    workspaces.remove(slot);
    return commitPlacementData(workspaces, monitorData(),
        tr("Workspace %1 removed from the plan.").arg(slot));
}

QVariantList Backend::monitorPlan() const
{
    QVariantList result;
    const QJsonObject saved = monitorData();
    for (const QVariant &value : m_monitors) {
        QVariantMap monitor = value.toMap();
        const QString description = monitor.value("description").toString();
        const QJsonObject entry = saved.value(description).toObject();
        QStringList modes;
        for (const QVariant &mode : monitor.value("availableModes").toList())
            modes << mode.toString().remove(QRegularExpression("Hz$"));
        const double refresh = monitor.value("refreshRate").toDouble();
        const QString current = QString("%1x%2@%3").arg(monitor.value("width").toInt())
            .arg(monitor.value("height").toInt()).arg(refresh, 0, 'f', 2);
        monitor.insert("description", description);
        monitor.insert("descKey", "desc:" + description);
        monitor.insert("modes", modes);
        monitor.insert("mode", entry.value("mode").toString().isEmpty() ? current : entry.value("mode").toString());
        monitor.insert("x", entry.value("x").toInt(monitor.value("x").toInt()));
        monitor.insert("y", entry.value("y").toInt(monitor.value("y").toInt()));
        monitor.insert("scale", entry.value("scale").toDouble(monitor.value("scale").toDouble()));
        monitor.insert("transform", entry.value("transform").toInt(monitor.value("transform").toInt()));
        monitor.insert("saved", !entry.isEmpty());
        result.append(monitor);
    }
    return result;
}

bool Backend::applyMonitorLayout(const QString &description, const QVariantMap &fields)
{
    if (description.trimmed().isEmpty()) return false;
    if (!m_monitorRevertPending) m_monitorRevertState = monitorData();

    QJsonObject monitors = monitorData();
    QJsonObject entry = monitors.value(description).toObject();
    // The first change saves every connected screen, so the others keep the
    // position they already have.
    for (const QVariant &value : m_monitors) {
        const QVariantMap monitor = value.toMap();
        const QString name = monitor.value("description").toString();
        if (name.isEmpty() || monitors.contains(name)) continue;
        QJsonObject seed;
        seed.insert("mode", QString("%1x%2@%3").arg(monitor.value("width").toInt())
            .arg(monitor.value("height").toInt())
            .arg(monitor.value("refreshRate").toDouble(), 0, 'f', 2));
        seed.insert("x", monitor.value("x").toInt());
        seed.insert("y", monitor.value("y").toInt());
        seed.insert("scale", monitor.value("scale").toDouble());
        seed.insert("transform", monitor.value("transform").toInt());
        seed.insert("enabled", true);
        monitors.insert(name, seed);
    }
    if (fields.contains("mode") && !fields.value("mode").toString().trimmed().isEmpty())
        entry.insert("mode", fields.value("mode").toString().trimmed());
    if (!entry.contains("mode")) entry.insert("mode", "preferred");
    entry.insert("x", fields.value("x", entry.value("x").toInt(0)).toInt());
    entry.insert("y", fields.value("y", entry.value("y").toInt(0)).toInt());
    entry.insert("scale", qBound(0.25, fields.value("scale", entry.value("scale").toDouble(1)).toDouble(), 8.0));
    entry.insert("transform", qBound(0, fields.value("transform", entry.value("transform").toInt(0)).toInt(), 7));
    entry.insert("enabled", fields.value("enabled", true).toBool());
    monitors.insert(description, entry);

    if (!commitPlacementData(workspaceData(), monitors, tr("Display layout applied. Keep it within 15 seconds."), false))
        return false;
    m_monitorRevertPending = true;
    m_monitorRevertTimer.start(15000);
    emit monitorRevertChanged();
    return true;
}

bool Backend::keepMonitorLayout()
{
    m_monitorRevertTimer.stop();
    m_monitorRevertState = {};
    if (m_monitorRevertPending) {
        m_monitorRevertPending = false;
        emit monitorRevertChanged();
        setStatus(tr("Display layout kept."));
    }
    return true;
}

bool Backend::revertMonitorLayout()
{
    m_monitorRevertTimer.stop();
    if (!m_monitorRevertPending) return true;
    const QJsonObject previous = m_monitorRevertState;
    m_monitorRevertPending = false;
    m_monitorRevertState = {};
    const bool ok = commitPlacementData(workspaceData(), previous, tr("Display layout reverted."), false);
    emit monitorRevertChanged();
    return ok;
}

bool Backend::commitPlacementData(const QJsonObject &workspaces, const QJsonObject &monitors,
                                  const QString &status, bool keepHistory, const QJsonArray &rules)
{
    QJsonArray effective = rules;
    if (effective.isEmpty()) {
        QString error;
        if (!readRuleData(&effective, &error)) { setStatus(error); return false; }
    }
    const QJsonObject oldLibrary = m_library;
    const QByteArray oldBytes = m_libraryBytes;
    QJsonObject library = m_library;
    library.insert("workspaces", workspaces);
    library.insert("monitors", monitors);
    if (!writeLibrary(library)) return false;
    if (!commitRuleData(effective, false, keepHistory)) {
        // commitRuleData restored the rule data and generated module; put the
        // library back too so its bytes match what Hyprland just loaded.
        const QString path = m_configDirectory + "/windowtuner-library.json";
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly) && file.write(oldBytes) == oldBytes.size() && file.commit())
            QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
        m_library = oldLibrary;
        m_libraryBytes = oldBytes;
        emit libraryChanged();
        return false;
    }
    emit workspacePlanChanged();
    emit catalogChanged();
    if (!status.isEmpty()) setStatus(status);
    return true;
}

QJsonObject Backend::appSpaceModuleConfig(const QJsonArray &rules, const QJsonObject &workspaces,
                                          const QJsonObject &monitors)
{
    QJsonObject config;
    QJsonObject workspaceOut;
    for (auto it = workspaces.cbegin(); it != workspaces.cend(); ++it) {
        const QJsonObject entry = it.value().toObject();
        workspaceOut.insert(it.key(), QJsonObject{{"monitor", entry.value("monitor").toString()},
            {"persistent", entry.value("persistent").toBool()}});
    }
    config.insert("workspaces", workspaceOut);
    config.insert("monitors", monitors);

    QJsonArray startup;
    QJsonArray autostart;
    for (const QJsonValue &value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.contains("enabled") && !rule.value("enabled").toBool()) continue;
        const QJsonObject settings = rule.value("settings").toObject();
        const QString workspace = settings.value("workspace").toString().trimmed();
        const QString klass = rule.value("class").toString();
        if (settings.value("startupOnly").toBool() && !workspace.isEmpty()) {
            const QJsonObject entry = workspaces.value(workspace).toObject();
            startup.append(QJsonObject{{"class", klass}, {"workspace", workspace},
                {"monitor", entry.value("monitor").toString()},
                {"follow", !settings.value("noInitialFocus").toBool()},
                {"within", qBound(1, settings.value("startupWindow").toInt(20), 604800)}});
        }
        if (settings.value("autostart").toBool()) {
            QString command = settings.value("launchCommand").toString();
            const QString desktop = settings.value("launchDesktop").toString();
            if (command.isEmpty() && !desktop.isEmpty())
                command = "gtk-launch " + quoted(desktop);
            if (command.isEmpty()) continue;
            QJsonObject entry{{"command", command}};
            if (!desktop.isEmpty()) entry.insert("omarchy", "uwsm-app -- " + quoted(desktop));
            autostart.append(entry);
        }
    }
    if (!startup.isEmpty()) config.insert("startup", startup);
    if (!autostart.isEmpty()) config.insert("autostart", autostart);
    return config;
}

QVariantMap Backend::appSpaceImportPreview(const QString &path) const
{
    const QString file = appSpaceImportPath(path);
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly))
        return {{"ok", false}, {"error", tr("Could not read %1.").arg(file)}};
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return {{"ok", false}, {"error", tr("That is not an AppSpace rules file.")}};
    const QJsonObject source = document.object();
    const QJsonArray sourceRules = source.value("rules").toArray();
    const QJsonObject sourceWorkspaces = source.value("workspaces").toObject();
    const QJsonObject sourceMonitors = source.value("monitors").toObject();
    if (sourceRules.isEmpty() && sourceWorkspaces.isEmpty() && sourceMonitors.isEmpty())
        return {{"ok", false}, {"error", tr("The file has no rules, workspaces or displays to import.")}};

    QJsonArray existing;
    QString error;
    readRuleData(&existing, &error);
    QSet<QString> classes;
    for (const QJsonValue &value : existing) {
        const QJsonObject rule = value.toObject();
        classes.insert(rule.value("class").toString().toLower());
    }
    // The same alias resolution the import uses, so the preview's counts match
    // what a second import would actually do.
    const QJsonObject sourceAliases = source.value("aliases").toObject();
    const auto aliased = [&sourceAliases](const QString &klass) {
        if (sourceAliases.contains(klass)) return sourceAliases.value(klass).toString();
        for (auto it = sourceAliases.cbegin(); it != sourceAliases.cend(); ++it)
            if (it.key().compare(klass, Qt::CaseInsensitive) == 0) return it.value().toString();
        return klass;
    };
    int added = 0, updated = 0;
    QVariantList sample;
    for (const QJsonValue &value : sourceRules) {
        const QJsonObject rule = value.toObject();
        const QString klass = aliased(rule.value("class").toString());
        if (klass.isEmpty()) continue;
        if (classes.contains(klass.toLower())) ++updated; else ++added;
        if (sample.size() < 8)
            sample.append(QVariantMap{{"class", klass}, {"name", rule.value("label").toString(klass)},
                {"workspace", rule.value("workspace").toString()},
                {"silent", rule.value("silent").toBool()},
                {"autostart", rule.value("autostart").toBool()}});
    }
    return {{"ok", true}, {"path", file}, {"added", added}, {"updated", updated},
        {"workspaces", sourceWorkspaces.size()}, {"monitors", sourceMonitors.size()},
        {"sample", sample}, {"note", tr("Nothing has been written yet.")}};
}

bool Backend::importAppSpace(const QString &path)
{
    const QString file = appSpaceImportPath(path);
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly)) { setStatus(tr("Could not read %1.").arg(file)); return false; }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setStatus(tr("That is not an AppSpace rules file.")); return false;
    }
    const QJsonObject source = document.object();

    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }

    QHash<QString, int> existing;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        const QString klass = rule.value("class").toString();
        if (!existing.contains(klass.toLower())) existing.insert(klass.toLower(), i);
    }

    // AppSpace records the class it saw at first launch and later corrects it
    // with an alias. Resolve the alias before matching, so importing twice
    // updates the same rule instead of adding a second one for the old name.
    const QJsonObject sourceAliases = source.value("aliases").toObject();
    const auto aliased = [&sourceAliases](const QString &klass) {
        if (sourceAliases.contains(klass)) return sourceAliases.value(klass).toString();
        for (auto it = sourceAliases.cbegin(); it != sourceAliases.cend(); ++it)
            if (it.key().compare(klass, Qt::CaseInsensitive) == 0) return it.value().toString();
        return klass;
    };

    int imported = 0;
    for (const QJsonValue &value : source.value("rules").toArray()) {
        const QJsonObject incoming = value.toObject();
        const QString incomingClass = incoming.value("class").toString();
        if (incomingClass.isEmpty()) continue;
        const QString klass = aliased(incomingClass);
        const QString workspace = incoming.value("workspace").toString().trimmed();
        QJsonObject settings;
        int index = existing.value(klass.toLower(), -1);
        if (index < 0) index = existing.value(incomingClass.toLower(), -1);
        QJsonObject rule;
        if (index >= 0) {
            rule = rules.at(index).toObject();
            settings = rule.value("settings").toObject();
        } else {
            QJsonObject match{{"class", klass}};
            settings.insert("match", match);
            settings.insert("matchTitle", false);
            settings.insert("classMatchMode", "exact");
            settings.insert("titleMatchMode", "exact");
            settings.insert("layout", "keep");
            rule.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
            rule.insert("class", klass);
            rule.insert("title", "");
            rule.insert("match", match);
            rule.insert("enabled", true);
            rule.insert("appOrigin", "appspace");
        }
        // Keeps the saved class in step with the alias on re-import.
        rule.insert("class", klass);
        QJsonObject ruleMatch = rule.value("match").toObject();
        ruleMatch.insert("class", klass);
        rule.insert("match", ruleMatch);
        settings.insert("match", ruleMatch);
        settings.insert("workspace", workspace);
        settings.insert("noInitialFocus", incoming.value("silent").toBool());
        settings.insert("autostart", incoming.value("autostart").toBool());
        settings.insert("startupOnly", incoming.value("startupOnly").toBool() && !workspace.isEmpty());
        settings.insert("startupWindow", qBound(1, incoming.value("startupWindow").toInt(20), 604800));
        // AppSpace stores a launch target: a desktop id, or a shell command for
        // entries it could not resolve. Only a desktop id can go through
        // gtk-launch; anything else is kept verbatim.
        const QString command = incoming.value("command").toString();
        if (command.endsWith(".desktop"))
            settings.insert("launchDesktop", command);
        else
            settings.insert("launchDesktop", QString());
        settings.insert("launchCommand", command.isEmpty() ? QString()
            : command.endsWith(".desktop") ? "gtk-launch " + quoted(command) : command);
        rule.insert("settings", settings);
        rule.insert("name", incoming.value("label").toString(klass));
        rule.insert("verifiedClass", incoming.value("verified").toBool());
        if (index >= 0) rules.replace(index, rule); else rules.append(rule);
        ++imported;
    }

    QJsonObject workspaces = workspaceData();
    const QJsonObject sourceWorkspaces = source.value("workspaces").toObject();
    for (auto it = sourceWorkspaces.cbegin(); it != sourceWorkspaces.cend(); ++it) {
        const QJsonValue value = it.value();
        QJsonObject entry;
        if (value.isString()) {
            entry.insert("monitor", value.toString());
            entry.insert("persistent", false);
        } else {
            entry.insert("monitor", value.toObject().value("monitor").toString());
            entry.insert("persistent", value.toObject().value("persistent").toBool());
        }
        workspaces.insert(it.key(), entry);
    }
    QJsonObject monitors = monitorData();
    const QJsonObject sourceMonitors = source.value("monitors").toObject();
    for (auto it = sourceMonitors.cbegin(); it != sourceMonitors.cend(); ++it)
        monitors.insert(it.key(), it.value().toObject());
    if (!commitPlacementData(workspaces, monitors,
            tr("Imported %1 AppSpace rule(s).").arg(imported), true, rules))
        return false;
    emit catalogChanged();
    setStatus(tr("Imported %1 AppSpace rule(s), %2 display(s). AppSpace's own module is still loaded; remove it before relying on these rules.")
                  .arg(imported).arg(monitors.size()));
    return true;
}
