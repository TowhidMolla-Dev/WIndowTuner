#include "backend.h"

#include <QDir>
#include <QHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QLockFile>
#include <QDateTime>
#include <QCryptographicHash>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace {
QString luaString(QString value)
{
    value.replace('\\', "\\\\");
    value.replace('"', "\\\"");
    value.replace('\n', "\\n");
    value.replace('\r', "\\r");
    return '"' + value + '"';
}

QString regexEscape(QString value)
{
    static const QString special = R"(\^$.*+?()[]{}|)";
    QString escaped;
    escaped.reserve(value.size() * 2);
    for (const QChar ch : value) {
        if (special.contains(ch)) escaped += '\\';
        escaped += ch;
    }
    return escaped;
}

QString patternFor(const QString &value, const QString &mode)
{
    if (mode == "regex") return value;
    const QString escaped = regexEscape(value);
    return mode == "contains" ? ".*" + escaped + ".*" : "^" + escaped + "$";
}

QString luaValue(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) return "nil";
    if (value.metaType().id() == QMetaType::Bool) return value.toBool() ? "true" : "false";
    if (value.metaType().id() == QMetaType::QVariantList) {
        QStringList values;
        for (const QVariant &entry : value.toList()) values.append(luaValue(entry));
        return "{ " + values.join(", ") + " }";
    }
    if (value.metaType().id() == QMetaType::QVariantMap) {
        QStringList values;
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            values << "[" + luaString(it.key()) + "] = " + luaValue(it.value());
        return "{ " + values.join(", ") + " }";
    }
    if (value.metaType().id() == QMetaType::Int || value.metaType().id() == QMetaType::LongLong ||
        value.metaType().id() == QMetaType::Double || value.metaType().id() == QMetaType::UInt)
        return value.toString();
    return luaString(value.toString());
}

QString number(const QVariantMap &map, const QString &key, double fallback)
{
    bool ok = false;
    const double value = map.value(key).toDouble(&ok);
    QString result = QString::number(ok ? value : fallback, 'f', 2);
    while (result.endsWith('0')) result.chop(1);
    if (result.endsWith('.')) result.chop(1);
    return result;
}

bool writeAtomically(const QString &path, const QByteArray &data, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = "Could not create " + info.absolutePath();
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
}

Backend::Backend(QObject *parent)
    : Backend(QDir::homePath() + "/.config/hypr",
              QDir::homePath() + "/.config/hypr/hyprland.lua",
              QStandardPaths::findExecutable("hyprctl"), parent)
{
    const QString configured = m_library.value("entryPoint").toString();
    if (!configured.isEmpty() && QFileInfo(configured).isAbsolute() && QFileInfo(configured).isReadable()) {
        m_hyprlandConfig = configured;
        m_diskFingerprint = fingerprint();
    }
}

Backend::Backend(const QString &configDirectory, const QString &hyprlandConfig,
                 const QString &hyprctl, QObject *parent)
    : QObject(parent), m_configDirectory(configDirectory),
      m_hyprlandConfig(hyprlandConfig), m_hyprctl(hyprctl),
      m_rulePath(configDirectory + "/windowtuner.lua")
{
    QJsonArray saved;
    QString dataError;
    if (readRuleData(&saved, &dataError)) updateSavedRules(saved);
    m_diskFingerprint = fingerprint();
    loadJournal();
    loadLibrary();
    m_trialTimer.setSingleShot(true);
    connect(&m_trialTimer, &QTimer::timeout, this, &Backend::revertTrial);
    m_monitorRevertTimer.setSingleShot(true);
    connect(&m_monitorRevertTimer, &QTimer::timeout, this, &Backend::revertMonitorLayout);
    refreshWindows();
    if (!dataError.isEmpty()) setStatus(dataError);
    m_refreshTimer.setInterval(1800);
    connect(&m_refreshTimer, &QTimer::timeout, this, &Backend::refreshWindows);
    m_refreshTimer.start();
    m_sessionSnapshotTimer.setSingleShot(true);
    m_sessionSnapshotTimer.setInterval(1800);
    connect(&m_sessionSnapshotTimer, &QTimer::timeout, this, &Backend::saveSessionSnapshot);
    connectHyprlandEvents();
}

void Backend::connectHyprlandEvents()
{
    const QString instance = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (instance.isEmpty() || runtime.isEmpty()) return;
    const QString socketPath = runtime + "/hypr/" + instance + "/.socket2.sock";
    if (!m_eventSocket.property("eventHandlersConnected").toBool()) {
      m_eventSocket.setProperty("eventHandlersConnected", true);
      connect(&m_eventSocket, &QLocalSocket::readyRead, this, [this] {
        m_eventBuffer += m_eventSocket.readAll();
        while (true) {
            const qsizetype newline = m_eventBuffer.indexOf('\n');
            if (newline < 0) break;
            const QByteArray event = m_eventBuffer.left(newline).trimmed();
            m_eventBuffer.remove(0, newline + 1);
            const QByteArray name = event.left(event.indexOf(">>"));
            if (name == "openwindow" || name == "closewindow" || name == "movewindow" ||
                name == "movewindowv2" || name == "changefloatingmode" ||
                name == "fullscreen" || name == "monitoradded" || name == "monitorremoved" ||
                name == "workspace" || name == "workspacev2" || name == "focusedmon")
                handleHyprlandEvent();
        }
        if (m_eventBuffer.size() > 65536) m_eventBuffer.clear();
      });
      connect(&m_eventSocket, &QLocalSocket::disconnected, this, [this] {
        m_eventBuffer.clear();
        QTimer::singleShot(2500, this, [this] {
            if (m_eventSocket.state() == QLocalSocket::UnconnectedState)
                connectHyprlandEvents();
        });
      });
    }
    m_eventSocket.connectToServer(socketPath, QIODevice::ReadOnly);
}

void Backend::handleHyprlandEvent()
{
    refreshWindows();
    if (sessionSnapshotsEnabled()) m_sessionSnapshotTimer.start();
}

QVariantList Backend::parseClients(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) *error = parseError.errorString();
        return {};
    }

    QVariantList result;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject client = value.toObject();
        QVariantMap row;
        row.insert("address", client.value("address").toString());
        row.insert("class", client.value("initialClass").toString(client.value("class").toString()));
        row.insert("currentClass", client.value("class").toString());
        row.insert("title", client.value("initialTitle").toString(client.value("title").toString()));
        row.insert("currentTitle", client.value("title").toString());
        row.insert("workspace", client.value("workspace").toObject().value("name").toString());
        row.insert("monitor", client.value("monitor").toInt());
        row.insert("monitorName", client.value("monitorName").toString());
        row.insert("at", client.value("at").toArray().toVariantList());
        row.insert("size", client.value("size").toArray().toVariantList());
        row.insert("pid", client.value("pid").toInt());
        row.insert("floating", client.value("floating").toBool());
        row.insert("xwayland", client.value("xwayland").toBool());
        row.insert("fullscreen", client.value("fullscreen").toInt());
        row.insert("pinned", client.value("pinned").toBool());
        row.insert("grouped", !client.value("grouped").toArray().isEmpty());
        row.insert("stableId", client.value("stableId").toString());
        row.insert("xdgTag", client.value("xdgTag").toString());
        row.insert("contentType", client.value("contentType").toString());
        row.insert("focusHistoryID", client.value("focusHistoryID").toInt(999999));
        result.append(row);
    }
    return result;
}

void Backend::refreshWindows()
{
    if (m_hyprctl.isEmpty()) {
        setStatus("hyprctl was not found. Run this app inside a Hyprland session.");
        return;
    }
    QProcess process;
    process.start(m_hyprctl, {"-j", "clients"});
    if (!process.waitForStarted(1500) || !process.waitForFinished(3000) || process.exitCode() != 0) {
        setStatus("Could not query Hyprland windows: " + QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        return;
    }
    QString error;
    QVariantList clients = parseClients(process.readAllStandardOutput(), &error);
    if (!error.isEmpty()) {
        setStatus("Hyprland returned invalid window data: " + error);
        return;
    }
    // Add readable monitor identities without asking the UI to issue another query.
    m_monitors.clear();
    QProcess monitorProcess;
    monitorProcess.start(m_hyprctl, {"-j", "monitors"});
    if (monitorProcess.waitForStarted(1000) && monitorProcess.waitForFinished(2000) && monitorProcess.exitCode() == 0) {
        const QJsonArray monitorArray = QJsonDocument::fromJson(monitorProcess.readAllStandardOutput()).array();
        m_monitors = monitorArray.toVariantList();
        QHash<int, QString> names;
        for (const QJsonValue &value : monitorArray) {
            const QJsonObject monitor = value.toObject();
            names.insert(monitor.value("id").toInt(), monitor.value("name").toString());
        }
        for (QVariant &entry : clients) {
            QVariantMap row = entry.toMap();
            row.insert("monitorName", names.value(row.value("monitor").toInt(), QString::number(row.value("monitor").toInt())));
            entry = row;
        }
    }
    const QString selectedAddress = selectedWindow().value("address").toString();
    clients.erase(std::remove_if(clients.begin(), clients.end(), [](const QVariant &entry) {
        return entry.toMap().value("class").toString() == "windowtuner";
    }), clients.end());
    const int previousCount = m_windows.size();
    m_windows = clients;
    m_selectedIndex = -1;
    for (int i = 0; i < clients.size(); ++i)
        if (clients.at(i).toMap().value("address").toString() == selectedAddress) { m_selectedIndex = i; break; }
    if (m_selectedIndex < 0 && !clients.isEmpty() && m_offlineTarget.isEmpty()) m_selectedIndex = 0;
    emit windowsChanged();
    emit selectedChanged();
    if (!m_hasRefreshed || previousCount != clients.size())
        setStatus(QString("Found %1 open window(s). Choose one to make a rule for it.").arg(clients.size()));
    m_hasRefreshed = true;
}

bool Backend::pickWindowAt(int x, int y)
{
    int best = -1;
    int bestFocus = std::numeric_limits<int>::max();
    for (int i = 0; i < m_windows.size(); ++i) {
        const QVariantMap row = m_windows.at(i).toMap();
        const QVariantList at = row.value("at").toList();
        const QVariantList size = row.value("size").toList();
        if (at.size() != 2 || size.size() != 2) continue;
        const int left = at.at(0).toInt(), top = at.at(1).toInt();
        const int right = left + size.at(0).toInt(), bottom = top + size.at(1).toInt();
        if (x < left || x >= right || y < top || y >= bottom) continue;
        const int focus = row.value("focusHistoryID", 999999).toInt();
        if (focus < bestFocus) { best = i; bestFocus = focus; }
    }
    if (best < 0) {
        setStatus("No managed window was under that point. Try clicking its visible area.");
        return false;
    }
    selectWindow(best);
    setStatus(QString("Picked %1 — %2.").arg(selectedWindow().value("class").toString(), selectedWindow().value("currentTitle").toString()));
    return true;
}

void Backend::selectWindow(int index)
{
    if (index < 0 || index >= m_windows.size())
        return;
    if (m_selectedIndex == index && m_offlineTarget.isEmpty())
        return;
    m_offlineTarget.clear();
    m_selectedIndex = index;
    emit selectedChanged();
}

QString Backend::luaRule(const QVariantMap &window, const QVariantMap &settings,
                         const QJsonObject &workspaces)
{
    const QString klass = window.value("class").toString();
    const QString title = window.value("title").toString();
    if (klass.isEmpty())
        return "-- Cannot persist a rule: the selected window has no initial class.";

    QVariantMap matchValues = settings.value("match").toMap();
    if (matchValues.isEmpty()) {
        matchValues.insert("class", klass);
        if (settings.value("matchTitle", true).toBool() && !title.isEmpty())
            matchValues.insert("title", title);
    }
    QStringList match;
    const QStringList matchable{ "class", "title", "initial_class", "initial_title", "xdg_tag",
                                 "workspace", "xwayland", "fullscreen", "fullscreen_state_client",
                                 "fullscreen_state_internal", "float", "group", "modal", "pin", "content", "tag" };
    const QString classMode = settings.value("classMatchMode", "exact").toString();
    const QString titleMode = settings.value("titleMatchMode", "exact").toString();
    for (auto it = matchValues.cbegin(); it != matchValues.cend(); ++it) {
        if (!matchable.contains(it.key())) continue;
        QVariant value = it.value();
        if (it.key() == "class" || it.key() == "initial_class")
            value = patternFor(value.toString(), classMode);
        else if (it.key() == "title" || it.key() == "initial_title")
            value = patternFor(value.toString(), titleMode);
        match.append(QString("%1 = %2").arg(it.key(), luaValue(value)));
    }
    if (!settings.value("excludeTitle").toString().trimmed().isEmpty() && !matchValues.contains("title"))
        match.append(QString("title = %1").arg(luaString("negative:" + settings.value("excludeTitle").toString().trimmed())));

    QStringList effects;
    const auto placement = settings.value("placement").toMap();
    if (placement.value("enabled").toBool() && placement.value("runtimeRecovery").toBool()) {
        const auto digest = QCryptographicHash::hash(match.join(", ").toUtf8(), QCryptographicHash::Sha256).toHex().left(24);
        effects << "tag = " + luaString("+_windowtuner_" + QString::fromLatin1(digest));
    }
    const QString layout = settings.value("layout", "floating").toString();
    if (layout == "floating") {
        effects << "float = true";
        if (settings.value("center", true).toBool()) effects << "center = true";
        effects << QString("size = { %1, %2 }").arg(settings.value("width", 600).toInt()).arg(settings.value("height", 420).toInt());
        if (settings.value("setPosition", false).toBool())
            effects << QString("move = { %1, %2 }").arg(settings.value("x", 80).toInt()).arg(settings.value("y", 80).toInt());
    } else if (layout == "tiled") {
        effects << "tile = true";
    }
    if (settings.value("pin", false).toBool()) effects << "pin = true";
    if (settings.value("noBlur", false).toBool()) effects << "no_blur = true";
    if (settings.value("noShadow", false).toBool()) effects << "no_shadow = true";
    if (settings.value("noInitialFocus", false).toBool()) effects << "no_initial_focus = true";
    if (settings.value("noAnim", false).toBool()) effects << "no_anim = true";
    if (settings.value("noDim", false).toBool()) effects << "no_dim = true";
    if (settings.value("noVrr", false).toBool()) effects << "no_vrr = true";
    if (settings.value("persistentSize", false).toBool()) effects << "persistent_size = true";
    if (settings.value("roundingEnabled", false).toBool()) effects << QString("rounding = %1").arg(settings.value("rounding", 8).toInt());
    if (settings.value("minSizeEnabled", false).toBool())
        effects << QString("min_size = { %1, %2 }").arg(settings.value("minWidth", 320).toInt()).arg(settings.value("minHeight", 200).toInt());
    if (settings.value("maxSizeEnabled", false).toBool())
        effects << QString("max_size = { %1, %2 }").arg(settings.value("maxWidth", 1200).toInt()).arg(settings.value("maxHeight", 900).toInt());
    if (settings.value("opacityEnabled", false).toBool())
        effects << QString("opacity = %1").arg(luaString(number(settings, "opacityActive", 1.0) + " " + number(settings, "opacityInactive", 0.9)));
    if (settings.value("borderEnabled", false).toBool())
        effects << QString("border_size = %1").arg(settings.value("borderSize", 2).toInt());
    if (!settings.value("workspace", "").toString().trimmed().isEmpty() &&
        !settings.value("startupOnly", false).toBool()) {
        // A startup-only placement must not pin every later window of the app;
        // the generated runtime moves only the windows opened at login.
        QString workspace = settings.value("workspace").toString().trimmed();
        bool silentSuffix = false;
        if (workspace.endsWith(" silent")) { workspace.chop(7); silentSuffix = true; workspace = workspace.trimmed(); }
        const QString monitor = workspaces.value(workspace).toObject().value("monitor").toString();
        if (!monitor.isEmpty() && QRegularExpression("^([1-9]|10)$").match(workspace).hasMatch()) {
            // The slot means this monitor's own workspace on Omarchy 4.5; the
            // generated runtime resolves it from the monitor description.
            QString expression = "__windowtuner_ws(" + luaString(workspace) + ", " +
                luaString("desc:" + monitor) + ")";
            if (silentSuffix || settings.value("noInitialFocus", false).toBool())
                expression += " .. \" silent\"";
            effects << "workspace = " + expression;
        } else {
            effects << "workspace = " + luaString(settings.value("workspace").toString().trimmed());
        }
    }
    if (!settings.value("monitor").toString().isEmpty())
        effects << QString("monitor = %1").arg(luaString(settings.value("monitor").toString()));

    return QString("hl.window_rule({\n  match = { %1 },\n  %2\n})")
        .arg(match.join(", "), effects.join(", "));
}

QString Backend::previewRule(const QVariantMap &settings) const
{
    if (selectedWindow().value("class").toString().isEmpty())
        return "-- Select a window first.";
    const QVariantMap window = selectedWindow();
    if (settings.value("matchTitle", true).toBool() && window.value("title").toString().isEmpty() &&
        settings.value("match").toMap().value("title").toString().isEmpty())
        return "-- This window has no initial title. Uncheck title matching to deliberately make a class-wide rule.";
    return luaRule(window, settings, workspaceData());
}

QString Backend::liveApplyCode(const QVariantMap &window, const QVariantMap &settings)
{
    const QString address = window.value("address").toString();
    if (address.isEmpty()) return {};
    const QString selector = luaString("address:" + address);
    QStringList commands;
    if (!settings.value("monitor").toString().isEmpty())
        commands << QString("hl.dispatch(hl.dsp.window.move({ window = %1, monitor = %2, follow = false }))")
                        .arg(selector, luaString(settings.value("monitor").toString()));
    const QString layout = settings.value("layout", "floating").toString();
    if (layout == "floating" || layout == "tiled") {
        commands << QString("hl.dispatch(hl.dsp.window.float({ window = %1, action = %2 }))")
                        .arg(selector, luaString(layout == "floating" ? "on" : "off"));
    }
    if (layout == "floating") {
        commands << QString("hl.dispatch(hl.dsp.window.resize({ window = %1, x = %2, y = %3, relative = false }))")
                        .arg(selector).arg(settings.value("width", 600).toInt()).arg(settings.value("height", 420).toInt());
        if (settings.value("center", true).toBool())
            commands << QString("hl.dispatch(hl.dsp.window.center({ window = %1 }))").arg(selector);
        else if (settings.value("setPosition", false).toBool())
            commands << QString("hl.dispatch(hl.dsp.window.move({ window = %1, x = %2, y = %3, relative = false }))")
                            .arg(selector).arg(settings.value("x", 80).toInt()).arg(settings.value("y", 80).toInt());
    }
    if (settings.value("pin", false).toBool())
        commands << QString("hl.dispatch(hl.dsp.window.pin({ window = %1, action = \"on\" }))").arg(selector);
    QString workspace = settings.value("workspace").toString().trimmed();
    workspace.remove(QRegularExpression(R"(\s+silent$)"));
    if (!workspace.isEmpty())
        commands << QString("hl.dispatch(hl.dsp.window.move({ window = %1, workspace = %2, follow = false }))")
                        .arg(selector, luaString(workspace));
    const auto setProp = [&](const QString &property, const QString &value) {
        commands << QString("hl.dispatch(hl.dsp.window.set_prop({ window = %1, prop = %2, value = %3 }))")
                        .arg(selector, luaString(property), luaString(value));
    };
    if (settings.value("noBlur", false).toBool()) setProp("no_blur", "true");
    if (settings.value("noShadow", false).toBool()) setProp("no_shadow", "true");
    // Initial-focus is a creation-time effect. Setting no_focus live would
    // incorrectly prevent the user from focusing the window at all.
    if (settings.value("noAnim", false).toBool()) setProp("no_anim", "true");
    if (settings.value("noDim", false).toBool()) setProp("no_dim", "true");
    if (settings.value("noVrr", false).toBool()) setProp("no_vrr", "true");
    if (settings.value("persistentSize", false).toBool()) setProp("persistent_size", "true");
    if (settings.value("roundingEnabled", false).toBool()) setProp("rounding", QString::number(settings.value("rounding", 8).toInt()));
    if (settings.value("opacityEnabled", false).toBool())
        setProp("opacity", number(settings, "opacityActive", 1.0) + " " + number(settings, "opacityInactive", 0.9));
    if (settings.value("borderEnabled", false).toBool())
        setProp("border_size", QString::number(settings.value("borderSize", 2).toInt()));
    return commands.join("; ");
}

bool Backend::saveRule(const QVariantMap &settings)
{
    const auto problems = validateSettings(settings);
    if (!problems.isEmpty()) { setStatus(problems.join("\n")); return false; }
    if (selectedWindow().isEmpty()) {
        setStatus("Choose a window before saving a rule.");
        return false;
    }
    const QVariantMap window = selectedWindow();
    if (window.value("class").toString().isEmpty()) {
        setStatus("This window has no class, so it cannot be targeted safely.");
        return false;
    }
    if (settings.value("matchTitle", true).toBool() && window.value("title").toString().isEmpty() &&
        settings.value("match").toMap().value("title").toString().isEmpty()) {
        setStatus("This window has no initial title. Uncheck title matching to confirm a rule for every window of this class.");
        return false;
    }

    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }

    QVariantMap match = settings.value("match").toMap();
    if (match.isEmpty()) {
        match.insert("class", window.value("class").toString());
        if (settings.value("matchTitle", true).toBool()) match.insert("title", window.value("title").toString());
    }
    const QJsonObject matchObject = QJsonObject::fromVariantMap(match);
    QJsonObject saved;
    saved.insert("class", match.value("class", match.value("initial_class", window.value("class"))).toString());
    saved.insert("title", match.value("title").toString());
    saved.insert("match", QJsonObject::fromVariantMap(match));
    saved.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
    saved.insert("name", window.value("currentTitle").toString().isEmpty() ? window.value("class").toString() : window.value("currentTitle").toString());
    saved.insert("enabled", true);
    QVariantMap savedSettings = settings;
    savedSettings.insert("match", match);
    saved.insert("settings", QJsonObject::fromVariantMap(savedSettings));

    QJsonArray updated;
    bool replaced = false;
    for (const QJsonValue &value : rules) {
        const QJsonObject object = value.toObject();
        QJsonObject oldMatch = object.value("match").toObject();
        if (oldMatch.isEmpty()) {
            oldMatch.insert("class", object.value("class"));
            if (!object.value("title").toString().isEmpty()) oldMatch.insert("title", object.value("title"));
        }
        const auto oldSettings = object.value("settings").toObject();
        if (oldMatch == matchObject &&
            oldSettings.value("classMatchMode").toString("exact") == settings.value("classMatchMode", "exact").toString() &&
            oldSettings.value("titleMatchMode").toString("exact") == settings.value("titleMatchMode", "exact").toString()) {
            if (!replaced) {
                if (!object.value("id").toString().isEmpty()) saved.insert("id", object.value("id"));
                for (const auto &key : {"name", "folder", "tags"})
                    if (object.contains(key)) saved.insert(key, object.value(key));
                updated.append(saved);
            }
            replaced = true;
        } else updated.append(value);
    }
    if (!replaced) updated.append(saved);
    if (!commitRuleData(updated, true)) return false;
    if (window.value("address").toString().isEmpty()) {
        setStatus(tr("Persistent rule saved for an offline application. Open it to test the match."));
        return true;
    }
    QProcess live;
    live.start(m_hyprctl, {"eval", liveApplyCode(window, settings)});
    const bool finished = live.waitForStarted(1500) && live.waitForFinished(3000) && live.exitCode() == 0;
    const auto response = live.readAllStandardOutput().trimmed();
    if (!finished || (!response.isEmpty() && response != "ok")) {
        setStatus(QString("Saved the persistent rule, but could not update this window live: %1")
                      .arg(QString::fromLocal8Bit(live.readAllStandardError()).trimmed()));
        return false;
    }
    setStatus(QString("Saved a rule for %1%2. Hyprland reloaded.")
                  .arg(window.value("class").toString(),
                       saved.value("title").toString().isEmpty() ? QString(" (all windows of this class)") : " — " + saved.value("title").toString()));
    return true;
}

bool Backend::deleteRule(int index)
{
    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }
    if (index < 0 || index >= rules.size()) {
        setStatus("The selected saved rule is no longer available.");
        return false;
    }
    rules.removeAt(index);
    if (!commitRuleData(rules)) return false;
    setStatus("Saved rule removed. Hyprland reloaded.");
    return true;
}

bool Backend::readRuleData(QJsonArray *rules, QString *error) const
{
    QFile file(m_configDirectory + "/windowtuner.json");
    if (!file.exists()) { *rules = {}; return true; }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        if (error) *error = parseError.errorString();
        return false;
    }
    for (const auto &entry : doc.array()) {
        const auto rule = entry.toObject();
        if (!entry.isObject() || rule.value("class").toString().isEmpty() || !rule.value("settings").isObject()) {
            if (error) *error = tr("Every saved rule needs a class and settings object. Existing data was left untouched.");
            return false;
        }
    }
    *rules = doc.array();
    return true;
}

bool Backend::commitRuleData(const QJsonArray &rules, bool ensureInclude, bool recordHistory)
{
    QDir().mkpath(m_configDirectory);
    QLockFile lock(m_configDirectory + "/windowtuner.lock");
    if (!lock.tryLock(0)) { setStatus("Another WindowTuner operation is writing rules."); return false; }
    if (fingerprint() != m_diskFingerprint) {
        setStatus("Configuration changed outside WindowTuner. Reload from disk before saving; nothing was overwritten.");
        return false;
    }
    QFile journal(m_configDirectory + "/windowtuner-history.json");
    if ((journal.exists() && !journal.open(QIODevice::ReadOnly)) ||
        (journal.isOpen() && journal.readAll() != m_journalBytes) ||
        (!m_journalBytes.isEmpty() && !QJsonDocument::fromJson(m_journalBytes).isObject())) {
        setStatus(tr("History changed externally or is invalid. Restart or repair it before saving."));
        return false;
    }
    for (const auto &path : {m_configDirectory + "/windowtuner.json", m_rulePath, m_hyprlandConfig}) {
        QFile file(path);
        if (file.exists() && !file.open(QIODevice::ReadOnly)) {
            setStatus(tr("Cannot safely back up an unreadable configuration file.")); return false;
        }
    }
    if (trialActive() && !revertTrial()) return false;
    // Record the config errors that already exist. `configerrors` covers the
    // whole configuration, so without this baseline one unrelated broken file
    // would roll back every valid change.
    QString errorBaseline;
    QProcess baselineProcess;
    baselineProcess.start(m_hyprctl, {"configerrors"});
    if (baselineProcess.waitForStarted(1000) && baselineProcess.waitForFinished(2500) &&
        baselineProcess.exitCode() == 0)
        errorBaseline = QString::fromLocal8Bit(baselineProcess.readAllStandardOutput()).trimmed();
    const QJsonObject before = snapshot(tr("Before rule change"));
    const auto readBytes = [](const QString &path, bool *exists) {
        QFile file(path);
        *exists = file.exists();
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    bool hadData = false, hadModule = false, hadConfig = false;
    const QString dataPath = m_configDirectory + "/windowtuner.json";
    const QByteArray oldData = readBytes(dataPath, &hadData);
    const QByteArray oldModule = readBytes(m_rulePath, &hadModule);
    const QByteArray oldConfig = readBytes(m_hyprlandConfig, &hadConfig);
    const auto restore = [&]() {
        QString ignored;
        if (hadData) writeAtomically(dataPath, oldData, &ignored); else QFile::remove(dataPath);
        if (hadModule) writeAtomically(m_rulePath, oldModule, &ignored); else QFile::remove(m_rulePath);
        if (hadConfig) writeAtomically(m_hyprlandConfig, oldConfig, &ignored);
        QProcess rollback;
        rollback.start(m_hyprctl, {"reload"});
        rollback.waitForStarted(1000);
        rollback.waitForFinished(3000);
        QJsonArray previous;
        if (hadData && QJsonDocument::fromJson(oldData).isArray()) {
            previous = QJsonDocument::fromJson(oldData).array();
            updateSavedRules(previous);
        }
        else updateSavedRules({});
        m_diskFingerprint = fingerprint();
    };
    if (!writeRuleData(rules) || !writeRules(rules)) { restore(); return false; }
    if (ensureInclude && !ensureMainConfigIncludesRules()) { restore(); return false; }
    QProcess reload;
    reload.start(m_hyprctl, {"reload"});
    if (!reload.waitForStarted(1500) || !reload.waitForFinished(4000) || reload.exitCode() != 0) {
        const QString message = QString::fromLocal8Bit(reload.readAllStandardError()).trimmed();
        restore();
        setStatus("Hyprland reload failed; restored the previous rules and config. " + message);
        return false;
    }
    QProcess errors;
    errors.start(m_hyprctl, {"configerrors"});
    if (errors.waitForStarted(1000) && errors.waitForFinished(2500) && errors.exitCode() == 0) {
        const QByteArray output = errors.readAllStandardOutput().trimmed();
        const QString text = QString::fromLocal8Bit(output);
        const bool noErrors = output.isEmpty() || output == "[]" || text == "No config errors found.";
        if (!noErrors && text != errorBaseline) {
            restore();
            setStatus("Hyprland reported new config errors; restored the previous rules and config: " + text);
            return false;
        }
    } else {
        restore();
        setStatus("Could not validate Hyprland configuration; restored the previous rules.");
        return false;
    }
    updateSavedRules(rules);
    m_diskFingerprint = fingerprint();
    if (!m_replaying && recordHistory) {
        const auto previousPast = m_past, previousFuture = m_future;
        m_past.append(before);
        while (m_past.size() > 50) m_past.removeFirst();
        m_future = {};
        if (!saveJournal()) {
            m_past = previousPast; m_future = previousFuture;
            restore();
            setStatus(tr("Could not persist undo history; previous configuration restored."));
            return false;
        }
    }
    emit safetyChanged();
    m_lastSuccessfulSave = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    emit healthChanged();
    return true;
}

bool Backend::setRuleEnabled(int index, bool enabled)
{
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error) || index < 0 || index >= rules.size()) {
        setStatus("Could not update the rule: " + (error.isEmpty() ? QString("invalid rule index") : error));
        return false;
    }
    QJsonObject rule = rules.at(index).toObject();
    rule.insert("enabled", enabled);
    rules.replace(index, rule);
    if (!commitRuleData(rules)) return false;
    setStatus(enabled ? "Rule enabled and reloaded." : "Rule disabled and reloaded.");
    return true;
}

bool Backend::duplicateRule(int index)
{
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error) || index < 0 || index >= rules.size()) {
        setStatus("Could not duplicate the rule: " + (error.isEmpty() ? QString("invalid rule index") : error));
        return false;
    }
    QJsonObject copy = rules.at(index).toObject();
    copy.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
    copy.insert("name", copy.value("name").toString(copy.value("class").toString()) + " copy");
    copy.insert("enabled", true);
    rules.insert(index + 1, copy);
    if (!commitRuleData(rules)) return false;
    setStatus("Rule duplicated. Edit its match before enabling both copies.");
    return true;
}

bool Backend::moveRule(int index, int destination)
{
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error) || index < 0 || index >= rules.size() || destination < 0 || destination >= rules.size()) {
        setStatus("Could not reorder rules: invalid position.");
        return false;
    }
    const QJsonValue item = rules.takeAt(index);
    rules.insert(destination, item);
    if (!commitRuleData(rules)) return false;
    setStatus("Rule order updated.");
    return true;
}

bool Backend::updateRule(int index, const QVariantMap &settings)
{
    const auto problems = validateSettings(settings);
    if (!problems.isEmpty()) { setStatus(problems.join("\n")); return false; }
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error) || index < 0 || index >= rules.size()) {
        setStatus("Could not edit the rule: " + (error.isEmpty() ? QString("invalid rule index") : error));
        return false;
    }
    QJsonObject rule = rules.at(index).toObject();
    QVariantMap updatedSettings = settings;
    const auto match = settings.value("match", rule.value("match").toObject().toVariantMap()).toMap();
    if (match.value("class", match.value("initial_class")).toString().isEmpty()) {
        setStatus(tr("A safe class match is required.")); return false;
    }
    updatedSettings.insert("match", match);
    rule.insert("match", QJsonObject::fromVariantMap(match));
    rule.insert("class", match.value("class", match.value("initial_class")).toString());
    rule.insert("title", match.value("title", match.value("initial_title")).toString());
    rule.insert("settings", QJsonObject::fromVariantMap(updatedSettings));
    rules.replace(index, rule);
    if (!commitRuleData(rules)) return false;
    setStatus("Rule changes saved and Hyprland reloaded.");
    return true;
}

QVariantMap Backend::loadRuleForEditing(int index)
{
    if (index < 0 || index >= m_savedRules.size()) {
        setStatus("Saved rule is no longer available.");
        return {};
    }
    const QVariantMap rule = m_savedRules.at(index).toMap();
    const QVariantMap match = rule.value("match").toMap();
    const QString klass = match.value("class", rule.value("class")).toString();
    const QString title = match.value("title", rule.value("title")).toString();
    auto settings = rule.value("settings").toMap();
    settings.insert("match", match);
    int candidate = -1;
    for (int i = 0; i < m_windows.size(); ++i) {
        const QVariantMap window = m_windows.at(i).toMap();
        if (!matchWindow(window, settings).value("matched").toBool()) continue;
        candidate = i;
        break;
    }
    if (candidate < 0) {
        m_offlineTarget = {{"class", klass}, {"title", title}, {"currentTitle", rule.value("name")},
                           {"offline", true}, {"workspace", ""}, {"xdgTag", ""}, {"contentType", ""},
                           {"size", QVariantList{0, 0}}, {"at", QVariantList{0, 0}}};
        m_selectedIndex = -1;
        emit selectedChanged();
        setStatus("Rule loaded, but no matching window is open; select its window before editing the match.");
    } else {
        m_offlineTarget.clear();
        m_selectedIndex = candidate;
        emit selectedChanged();
        setStatus("Loaded saved rule into the editor. Save changes to update this rule.");
    }
    return settings;
}

QVariantMap Backend::editorStateForWindow(int index) const
{
    if (index < 0 || index >= m_windows.size()) return {};
    const QVariantMap window = m_windows.at(index).toMap();
    QVariantList matches;
    for (int i = 0; i < m_savedRules.size(); ++i) {
        const QVariantMap rule = m_savedRules.at(i).toMap();
        if (!rule.value("enabled", true).toBool()) continue;
        QVariantMap settings = rule.value("settings").toMap();
        QVariantMap match = rule.value("match").toMap();
        if (match.isEmpty()) {
            match.insert("class", rule.value("class"));
            if (!rule.value("title").toString().isEmpty()) match.insert("title", rule.value("title"));
        }
        settings.insert("match", match);
        if (!matchWindow(window, settings).value("matched").toBool()) continue;
        int specificity = match.size();
        if (match.contains("title")) specificity += 8;
        if (settings.value("classMatchMode", "exact").toString() == "exact") specificity += 2;
        if (settings.value("titleMatchMode", "exact").toString() == "exact" && match.contains("title"))
            specificity += 4;
        matches.append(QVariantMap{{"index", i}, {"id", rule.value("id")},
            {"name", rule.value("name", rule.value("class"))}, {"settings", settings},
            {"specificity", specificity}});
    }
    std::sort(matches.begin(), matches.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap a = left.toMap(), b = right.toMap();
        const int scoreA = a.value("specificity").toInt(), scoreB = b.value("specificity").toInt();
        return scoreA == scoreB ? a.value("index").toInt() < b.value("index").toInt()
                                : scoreA > scoreB;
    });
    QVariantMap result{{"window", window}, {"matchingRuleCount", matches.size()},
        {"matchingRules", matches}};
    if (!matches.isEmpty()) {
        const QVariantMap best = matches.first().toMap();
        result.insert("ruleIndex", best.value("index"));
        result.insert("ruleId", best.value("id"));
        result.insert("ruleName", best.value("name"));
        result.insert("settings", best.value("settings"));
    } else {
        result.insert("ruleIndex", -1);
    }
    return result;
}

bool Backend::applySavedRule(int index)
{
    if (index < 0 || index >= m_savedRules.size()) {
        setStatus("Saved rule is no longer available.");
        return false;
    }
    const QVariantMap rule = m_savedRules.at(index).toMap();
    if (!rule.value("enabled", true).toBool()) { setStatus(tr("Enable the rule before applying it.")); return false; }
    QVariantMap settings = resolvePlacement(rule.value("settings").toMap());
    settings.insert("match", rule.value("match"));
    if (!validateSettings(settings).isEmpty()) { setStatus(validateSettings(settings).join("\n")); return false; }
    int applied = 0;
    for (const QVariant &entry : m_windows) {
        const QVariantMap window = entry.toMap();
        if (!matchWindow(window, settings).value("matched").toBool()) continue;
        QProcess live;
    live.start(m_hyprctl, {"eval", liveApplyCode(window, settings)});
    const bool liveFinished = live.waitForStarted(1500) && live.waitForFinished(3000) && live.exitCode() == 0;
    const auto liveResponse = live.readAllStandardOutput().trimmed();
    if (!liveFinished || (!liveResponse.isEmpty() && liveResponse != "ok")) {
            setStatus("Could not apply the saved rule: " + QString::fromLocal8Bit(live.readAllStandardError()).trimmed());
            return false;
        }
        ++applied;
    }
    setStatus(applied ? QString("Applied to %1 matching open window(s).").arg(applied) : "No currently open windows match this rule.");
    return applied > 0;
}

QString Backend::exportRules() const
{
    QFile file(m_configDirectory + "/windowtuner.json");
    if (!file.open(QIODevice::ReadOnly)) return "[]";
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isArray() ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented)) : "[]";
}

bool Backend::importRules(const QString &json)
{
    QJsonArray rules;
    QString error;
    if (!decodeRules(json, &rules, &error)) { setStatus(error); return false; }
    if (!commitRuleData(rules, true)) return false;
    setStatus(QString("Imported %1 rule(s).").arg(rules.size()));
    return true;
}

QString Backend::ruleModule(const QJsonArray &rules, const QJsonObject &workspaces,
                            const QJsonObject &monitors)
{
    QStringList blocks;
    QVariantList policies;
    for (const QJsonValue &value : rules) {
        const QJsonObject object = value.toObject();
        if (object.contains("enabled") && !object.value("enabled").toBool()) continue;
        QVariantMap window{{"class", object.value("class").toString()}, {"title", object.value("title").toString()}};
        QVariantMap settings = object.value("settings").toObject().toVariantMap();
        settings.insert("matchTitle", !window.value("title").toString().isEmpty());
        if (object.value("match").isObject())
            settings.insert("match", object.value("match").toObject().toVariantMap());
        const auto code = luaRule(window, settings, workspaces);
        blocks.append(code);
        const auto marker = QRegularExpression("tag = \"\\+(_windowtuner_[0-9a-f]+)\"").match(code);
        if (marker.hasMatch())
            policies.append(QVariantMap{{"tag", marker.captured(1)}, {"placement", settings.value("placement")}});
    }
    const QString cleanup = "if _G.__windowtuner_placement_runtime then\n"
        "  _G.__windowtuner_placement_runtime.stop()\n"
        "  _G.__windowtuner_placement_runtime = nil\nend\n"
        "if _G.__windowtuner_appspace_runtime then\n"
        "  _G.__windowtuner_appspace_runtime.stop()\n"
        "  _G.__windowtuner_appspace_runtime = nil\nend\n"
        "_G.__windowtuner_ws = nil\n";
    QString result = "-- Generated by WindowTuner. Edit rules in the app.\n\n" + cleanup;

    const QJsonObject config = appSpaceModuleConfig(rules, workspaces, monitors);
    const bool needsRuntime = !workspaces.isEmpty() || !monitors.isEmpty() ||
        config.contains("startup") || config.contains("autostart");
    if (needsRuntime) {
        QFile source(":/appspace_runtime.lua");
        if (!source.open(QIODevice::ReadOnly)) return {};
        result += "\nlocal install_windowtuner_appspace = (function()\n" + QString::fromUtf8(source.readAll()) +
            "\nend)()\n_G.__windowtuner_appspace_runtime = install_windowtuner_appspace(" +
            luaValue(config.toVariantMap()) + ")\n_G.__windowtuner_ws = _G.__windowtuner_appspace_runtime and "
            "_G.__windowtuner_appspace_runtime.workspace_id\n";
    }

    result += "\n" + blocks.join("\n\n") + "\n";
    if (!policies.isEmpty()) {
        QFile source(":/monitor_recovery.lua");
        if (!source.open(QIODevice::ReadOnly)) return {};
        result += "\nlocal install_windowtuner_recovery = (function()\n" + QString::fromUtf8(source.readAll()) +
            "\nend)()\n_G.__windowtuner_placement_runtime = install_windowtuner_recovery(" + luaValue(policies) + ")\n";
    }
    return result;
}

bool Backend::writeRules(const QJsonArray &rules)
{
    const QString module = ruleModule(rules, workspaceData(), monitorData());
    if (module.isEmpty()) { setStatus(tr("Could not load the bundled monitor recovery runtime.")); return false; }
    QString error;
    if (!writeAtomically(m_rulePath, module.toUtf8(), &error)) {
        setStatus("Could not write the rules module: " + error);
        return false;
    }
    return true;
}

bool Backend::writeRuleData(const QJsonArray &rules)
{
    QString error;
    const QByteArray data = QJsonDocument(rules).toJson(QJsonDocument::Indented);
    if (!writeAtomically(m_configDirectory + "/windowtuner.json", data, &error)) {
        setStatus("Could not save rule data: " + error);
        return false;
    }
    return true;
}

void Backend::updateSavedRules(const QJsonArray &rules)
{
    QVariantList result;
    for (const QJsonValue &value : rules) {
        QVariantMap rule = value.toObject().toVariantMap();
        rule.insert("settings", value.toObject().value("settings").toObject().toVariantMap());
        result.append(rule);
    }
    for (int i = 0; i < result.size(); ++i) {
        QVariantMap rule = result.at(i).toMap();
        const QVariantMap match = rule.value("match").toMap();
        QStringList overlaps;
        for (int j = 0; j < result.size(); ++j) {
            if (i == j) continue;
            const QVariantMap other = result.at(j).toMap();
            const QVariantMap otherMatch = other.value("match").toMap();
            const QString klass = match.value("class", rule.value("class")).toString();
            const QString otherClass = otherMatch.value("class", other.value("class")).toString();
            if (klass.isEmpty() || klass != otherClass) continue;
            const QString title = match.value("title").toString();
            const QString otherTitle = otherMatch.value("title").toString();
            const QString mode = rule.value("settings").toMap().value("classMatchMode", "exact").toString();
            const QString otherMode = other.value("settings").toMap().value("classMatchMode", "exact").toString();
            if (title.isEmpty() || otherTitle.isEmpty() || title == otherTitle || mode != "exact" || otherMode != "exact")
                overlaps.append(other.value("name", other.value("class")).toString());
        }
        rule.insert("conflict", !overlaps.isEmpty());
        rule.insert("conflictsWith", overlaps);
        result[i] = rule;
    }
    m_savedRules = result;
    emit savedRulesChanged();
}

bool Backend::ensureMainConfigIncludesRules()
{
    QFile file(m_hyprlandConfig);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus("Rules were saved, but could not read " + m_hyprlandConfig + ". Add require(\"hypr.windowtuner\") to your Hyprland Lua config.");
        return false;
    }
    QByteArray content = file.readAll();
    const QByteArray include = moduleInclude().toUtf8();
    if (content.contains(include) || content.contains("require(\"hypr.windowtuner\")") || content.contains("require('hypr.windowtuner')"))
        return true;

    const QString backupPath = m_hyprlandConfig + ".windowtuner.bak";
    if (!QFileInfo::exists(backupPath) && !QFile::copy(m_hyprlandConfig, backupPath)) {
        setStatus("Could not make a backup of hyprland.lua; no config changes were made.");
        return false;
    }
    if (!content.endsWith('\n')) content.append('\n');
    content.append("\n-- Load per-window rules saved by WindowTuner.\n" + include + "\n");
    QString error;
    if (!writeAtomically(m_hyprlandConfig, content, &error)) {
        setStatus("Rule file was saved, but hyprland.lua could not be updated: " + error);
        return false;
    }
    return true;
}

QString Backend::moduleInclude() const
{
    return "dofile(" + luaString(m_rulePath) + ")";
}

void Backend::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit statusChanged();
}
