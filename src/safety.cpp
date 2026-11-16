#include "backend.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <cmath>

namespace {
QByteArray bytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool atomic(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

bool run(const QString &program, const QStringList &args, QByteArray *output = nullptr)
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(1500) || !process.waitForFinished(4000) ||
        process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return false;
    const auto response = process.readAllStandardOutput().trimmed();
    if (!args.isEmpty() && args.first() == "eval" && !response.isEmpty() && response != "ok") return false;
    if (output) *output = response;
    return true;
}
}

Backend::~Backend()
{
    if (trialActive()) revertTrial();
}

QByteArray Backend::fingerprint() const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const QString &path : {m_configDirectory + "/windowtuner.json", m_rulePath, m_hyprlandConfig}) {
        hash.addData(QFile::exists(path) ? QByteArray("present") : QByteArray("absent"));
        hash.addData(bytes(path));
        hash.addData(QByteArray("\0", 1));
    }
    return hash.result();
}

QJsonObject Backend::snapshot(const QString &name) const
{
    QJsonObject state{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                      {"name", name.left(120)}, {"date", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                      {"entryPoint", m_hyprlandConfig}};
    const QStringList paths{m_configDirectory + "/windowtuner.json", m_rulePath, m_hyprlandConfig};
    const QStringList keys{"data", "module", "config"};
    for (int i = 0; i < paths.size(); ++i) {
        state.insert(keys[i], QString::fromLatin1(bytes(paths[i]).toBase64()));
        state.insert(keys[i] + "Exists", QFile::exists(paths[i]));
    }
    return state;
}

void Backend::loadJournal()
{
    m_journalBytes = bytes(m_configDirectory + "/windowtuner-history.json");
    const auto journal = QJsonDocument::fromJson(m_journalBytes).object();
    m_past = journal.value("past").toArray();
    m_future = journal.value("future").toArray();
    m_backups = journal.value("backups").toArray();
}

bool Backend::saveJournal()
{
    QDir().mkpath(m_configDirectory);
    QLockFile lock(m_configDirectory + "/windowtuner-history.lock");
    const QString path = m_configDirectory + "/windowtuner-history.json";
    if (!lock.tryLock(0) || bytes(path) != m_journalBytes ||
        (!m_journalBytes.isEmpty() && !QJsonDocument::fromJson(m_journalBytes).isObject())) {
        setStatus(tr("History changed externally or is invalid; it was not overwritten."));
        return false;
    }
    const auto data = QJsonDocument(QJsonObject{{"past", m_past}, {"future", m_future}, {"backups", m_backups}}).toJson();
    const bool ok = atomic(path, data);
    if (ok) m_journalBytes = data;
    QFile::setPermissions(m_configDirectory + "/windowtuner-history.json", QFile::ReadOwner | QFile::WriteOwner);
    if (!ok) setStatus(tr("Could not persist history. Rules may have saved; keep this session open to undo."));
    return ok;
}

QVariantList Backend::history() const
{
    QVariantList result;
    for (auto it = m_past.constEnd(); it != m_past.constBegin();) {
        const auto row = (*--it).toObject();
        result.append(QVariantMap{{"id", row.value("id").toString()}, {"name", row.value("name").toString()},
                                  {"date", row.value("date").toString()}});
    }
    return result;
}

QVariantList Backend::backups() const
{
    QVariantList result;
    for (const auto &entry : m_backups) {
        const auto row = entry.toObject();
        result.append(QVariantMap{{"id", row.value("id").toString()}, {"name", row.value("name").toString()},
                                  {"date", row.value("date").toString()}});
    }
    return result;
}

bool Backend::restoreSnapshot(const QJsonObject &state)
{
    QLockFile lock(m_configDirectory + "/windowtuner.lock");
    if (!lock.tryLock(0)) { setStatus(tr("Another operation is writing rules.")); return false; }
    if (fingerprint() != m_diskFingerprint) {
        setStatus(tr("External edits detected. Reload from disk before restoring.")); return false;
    }
    if (state.value("entryPoint").toString() != m_hyprlandConfig) {
        setStatus(tr("This snapshot belongs to another configuration entry point.")); return false;
    }
    for (const auto &key : {"data", "module", "config"}) {
        if (!state.value(key).isString() || !state.value(QString(key) + "Exists").isBool() ||
            state.value(key).toString().size() > 32 * 1024 * 1024) {
            setStatus(tr("Snapshot is incomplete or too large. No files were changed."));
            return false;
        }
        const auto decoded = QByteArray::fromBase64Encoding(state.value(key).toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) { setStatus(tr("Snapshot is corrupt. No files were changed.")); return false; }
    }
    if (trialActive() && !revertTrial()) return false;
    const auto current = snapshot(tr("Before restore"));
    const QStringList paths{m_configDirectory + "/windowtuner.json", m_rulePath, m_hyprlandConfig};
    const QStringList keys{"data", "module", "config"};
    const auto write = [&](const QJsonObject &snapshot) {
        bool ok = true;
        for (int i = 0; i < paths.size(); ++i) {
            if (snapshot.value(keys[i] + "Exists").toBool())
                ok = atomic(paths[i], QByteArray::fromBase64(snapshot.value(keys[i]).toString().toLatin1())) && ok;
            else if (QFile::exists(paths[i])) ok = QFile::remove(paths[i]) && ok;
        }
        return ok;
    };
    QByteArray errors;
    if (!write(state) || !run(m_hyprctl, {"reload"}) || !run(m_hyprctl, {"configerrors"}, &errors) ||
        (!errors.isEmpty() && errors != "[]" && errors != "No config errors found.")) {
        const bool restored = write(current);
        run(m_hyprctl, {"reload"});
        m_diskFingerprint = fingerprint();
        setStatus(restored ? tr("Restore failed validation; previous files restored.") :
                            tr("Restore AND recovery failed. Inspect configuration files before continuing."));
        return false;
    }
    m_diskFingerprint = fingerprint();
    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }
    updateSavedRules(rules);
    return true;
}

bool Backend::undo()
{
    if (m_past.isEmpty()) return false;
    const auto current = snapshot(tr("Redo rule change"));
    if (!restoreSnapshot(m_past.last().toObject())) return false;
    m_past.removeLast();
    m_future.append(current);
    saveJournal();
    emit safetyChanged();
    setStatus(tr("Undid the last configuration change. Reopen windows if runtime effects remain."));
    return true;
}

bool Backend::redo()
{
    if (m_future.isEmpty()) return false;
    const auto current = snapshot(tr("Before redone change"));
    if (!restoreSnapshot(m_future.last().toObject())) return false;
    m_future.removeLast();
    m_past.append(current);
    saveJournal();
    emit safetyChanged();
    setStatus(tr("Redid the configuration change."));
    return true;
}

bool Backend::createBackup(const QString &name)
{
    if (name.trimmed().isEmpty()) { setStatus(tr("Give the snapshot a name.")); return false; }
    for (const auto &path : {m_configDirectory + "/windowtuner.json", m_rulePath, m_hyprlandConfig}) {
        QFile file(path);
        if (file.exists() && !file.open(QIODevice::ReadOnly)) {
            setStatus(tr("Cannot snapshot an unreadable configuration file.")); return false;
        }
    }
    m_backups.append(snapshot(name.trimmed()));
    while (m_backups.size() > 30) m_backups.removeFirst();
    if (!saveJournal()) { m_backups.removeLast(); return false; }
    emit safetyChanged();
    setStatus(tr("Named snapshot created."));
    return true;
}

bool Backend::restoreBackup(const QString &id)
{
    for (const auto &entry : m_backups) {
        if (entry.toObject().value("id").toString() != id) continue;
        const auto before = snapshot(tr("Before snapshot restore"));
        if (!restoreSnapshot(entry.toObject())) return false;
        m_past.append(before);
        m_future = {};
        saveJournal();
        emit safetyChanged();
        setStatus(tr("Snapshot restored and configuration validated."));
        return true;
    }
    setStatus(tr("Snapshot not found."));
    return false;
}

bool Backend::reloadFromDisk()
{
    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) { setStatus(tr("Reload rejected: ") + error); return false; }
    updateSavedRules(rules);
    m_diskFingerprint = fingerprint();
    setStatus(tr("Reloaded external changes. Review your draft before saving."));
    return true;
}

QStringList Backend::validateSettings(const QVariantMap &s) const
{
    QStringList result;
    const auto errors = validationDetails(s);
    for (auto it = errors.cbegin(); it != errors.cend(); ++it) result << it.value().toString();
    return result;
}

QVariantMap Backend::validationDetails(const QVariantMap &s) const
{
    QVariantMap errors;
    const QStringList supported{
        "layout", "center", "width", "height", "setPosition", "x", "y", "matchTitle", "classMatchMode", "titleMatchMode",
        "excludeTitle", "match", "pin", "noBlur", "noShadow", "noInitialFocus", "noAnim", "noDim", "noVrr", "persistentSize",
        "roundingEnabled", "rounding", "minSizeEnabled", "minWidth", "minHeight", "maxSizeEnabled", "maxWidth", "maxHeight",
        "opacityEnabled", "opacityActive", "opacityInactive", "borderEnabled", "borderSize", "workspace", "placement", "error",
        "monitor", "usedFallback", "usableX", "usableY", "usableWidth", "usableHeight", "monitorWidth", "monitorHeight",
        "autostart", "launchCommand", "launchDesktop", "startupOnly", "startupWindow"};
    for (auto it = s.cbegin(); it != s.cend(); ++it)
        if (!supported.contains(it.key())) errors.insert(it.key(), tr("Unsupported setting: %1.").arg(it.key()));
    if (!s.value("error").toString().isEmpty()) errors.insert("placement", s.value("error"));
    if (s.value("startupOnly").toBool() && s.value("workspace").toString().trimmed().isEmpty())
        errors.insert("startupOnly", tr("Startup-only placement needs a workspace."));
    if (s.value("startupWindow").isValid()) {
        bool ok = false;
        const int seconds = s.value("startupWindow").toInt(&ok);
        if (!ok || seconds < 1 || seconds > 604800)
            errors.insert("startupWindow", tr("Startup window: enter 1 to 604800 seconds."));
    }
    if (s.value("autostart").toBool() && s.value("launchCommand").toString().trimmed().isEmpty() &&
        s.value("launchDesktop").toString().trimmed().isEmpty())
        errors.insert("autostart", tr("Launch at login needs a desktop entry or a command."));
    if (!QStringList{"floating", "tiled", "keep"}.contains(s.value("layout", "floating").toString()))
        errors.insert("layout", tr("Layout: select Floating, Tiled, or Keep current."));
    for (const auto &key : {"width", "height"}) {
        bool ok = false;
        const double n = s.value(key, 600).toDouble(&ok);
        if (!ok || !std::isfinite(n) || n < 1 || n > 10000)
            errors.insert(key, tr("%1: enter a number from 1 to 10000.").arg(key));
    }
    if (s.value("minSizeEnabled").toBool() && s.value("maxSizeEnabled").toBool() &&
        (s.value("minWidth", 320).toInt() > s.value("maxWidth", 1200).toInt() ||
         s.value("minHeight", 200).toInt() > s.value("maxHeight", 900).toInt()))
        errors.insert("constraints", tr("Size constraints: minimum must not exceed maximum."));
    for (const auto &key : {"opacityActive", "opacityInactive"}) {
        bool ok = false;
        const double n = s.value(key, 1.0).toDouble(&ok);
        if (!ok || !std::isfinite(n) || n < 0 || n > 1) errors.insert(key, tr("%1: use a value between 0 and 1.").arg(key));
    }
    const auto match = s.value("match").toMap();
    const QStringList matchable{"class", "title", "initial_class", "initial_title", "xdg_tag", "workspace", "xwayland",
        "fullscreen", "fullscreen_state_client", "fullscreen_state_internal", "float", "group", "modal", "pin", "content", "tag"};
    for (auto it = match.cbegin(); it != match.cend(); ++it)
        if (!matchable.contains(it.key())) errors.insert("match", tr("Unsupported match condition: %1.").arg(it.key()));
    for (const auto &key : {"class", "title"}) {
        if (!QStringList{"exact", "contains", "regex"}.contains(s.value(QString(key) + "MatchMode", "exact").toString()))
            errors.insert(key, tr("%1 match mode must be Exact, Contains, or Regular expression.").arg(key));
        if (s.value(QString(key) + "MatchMode") == "regex") {
            const QRegularExpression expression(match.value(key).toString());
            if (!expression.isValid()) errors.insert(key, tr("%1 expression: %2").arg(key, expression.errorString()));
        }
    }
    if (!s.value("excludeTitle").toString().isEmpty()) {
        QRegularExpression expression(s.value("excludeTitle").toString());
        if (!expression.isValid()) errors.insert("excludeTitle", tr("Title exclusion: %1").arg(expression.errorString()));
    }
    if (s.contains("match") && match.value("class", match.value("initial_class")).toString().trimmed().isEmpty())
        errors.insert("class", tr("A nonempty application class match is required."));
    for (const auto &key : {"minWidth", "minHeight", "maxWidth", "maxHeight"}) {
        if (!s.contains(key)) continue;
        bool ok = false; const int n = s.value(key).toInt(&ok);
        if (!ok || n < 1 || n > 10000) errors.insert("constraints", tr("Size limits must be between 1 and 10000."));
    }
    for (const auto &key : {"borderSize", "rounding"}) {
        if (!s.contains(key)) continue;
        bool ok = false; const int n = s.value(key).toInt(&ok);
        if (!ok || n < 0 || n > 100) errors.insert(key, tr("%1 must be between 0 and 100.").arg(key));
    }
    return errors;
}

QString Backend::summarize(const QVariantMap &s) const
{
    QStringList parts;
    parts << tr("Layout: %1").arg(s.value("layout", "floating").toString());
    if (s.value("layout", "floating") == "floating") {
        parts << tr("Size: %1 × %2 logical pixels").arg(s.value("width", 600).toInt()).arg(s.value("height", 420).toInt());
        if (s.value("center", true).toBool()) parts << tr("Centered on monitor");
        else if (s.value("setPosition").toBool()) parts << tr("Position: %1, %2").arg(s.value("x").toInt()).arg(s.value("y").toInt());
    }
    if (!s.value("workspace").toString().isEmpty()) parts << tr("Workspace: %1").arg(s.value("workspace").toString());
    if (!s.value("monitor").toString().isEmpty()) parts << tr("Monitor: %1%2").arg(s.value("monitor").toString(), s.value("usedFallback").toBool() ? tr(" (fallback)") : "");
    if (s.value("pin").toBool()) parts << tr("Pinned across workspaces");
    if (s.value("opacityEnabled").toBool()) parts << tr("Opacity: %1 focused / %2 unfocused").arg(s.value("opacityActive", 1).toDouble()).arg(s.value("opacityInactive", 0.9).toDouble());
    parts << (s.value("matchTitle", true).toBool() ? tr("Scope: application and title") : tr("Scope: application-wide"));
    const auto match = s.value("match").toMap();
    for (auto it = match.cbegin(); it != match.cend(); ++it)
        parts << tr("Match %1: %2").arg(it.key(), it.value().toString().left(160));
    parts << tr("Unchecked overrides are omitted; other rules may still apply. Some effects require reopening windows.");
    return parts.join("\n");
}

QVariantMap Backend::reviewChanges(const QVariantMap &settings, int index) const
{
    QVariantMap target = selectedWindow();
    QVariantMap effective = settings;
    if (index >= 0 && index < m_savedRules.size()) {
        target = m_savedRules[index].toMap();
        effective.insert("match", settings.value("match", target.value("match")));
    }
    auto match = effective.value("match").toMap();
    if (match.isEmpty()) {
        match.insert("class", target.value("class"));
        if (effective.value("matchTitle", true).toBool() && !target.value("title").toString().isEmpty())
            match.insert("title", target.value("title"));
    }
    effective.insert("match", match);
    QJsonArray rows;
    QString error;
    if (!readRuleData(&rows, &error)) return {{"errors", QStringList{error}}, {"summary", error}};
    QJsonObject proposed{{"class", target.value("class").toString()}, {"title", match.value("title").toString()},
        {"match", QJsonObject::fromVariantMap(match)}, {"settings", QJsonObject::fromVariantMap(effective)}, {"enabled", true}};
    if (index >= 0 && index < rows.size()) {
        auto existing = rows[index].toObject();
        existing.insert("match", proposed.value("match"));
        existing.insert("settings", proposed.value("settings"));
        existing.insert("class", proposed.value("class"));
        existing.insert("title", proposed.value("title"));
        rows.replace(index, existing);
    } else {
        QJsonArray updated;
        bool replaced = false;
        for (const auto &entry : rows) {
            const auto old = entry.toObject();
            auto oldMatch = old.value("match").toObject();
            if (oldMatch.isEmpty()) {
                oldMatch.insert("class", old.value("class"));
                if (!old.value("title").toString().isEmpty()) oldMatch.insert("title", old.value("title"));
            }
            const auto s = old.value("settings").toObject();
            if (oldMatch == proposed.value("match").toObject() &&
                s.value("classMatchMode").toString("exact") == effective.value("classMatchMode", "exact").toString() &&
                s.value("titleMatchMode").toString("exact") == effective.value("titleMatchMode", "exact").toString()) {
                if (!replaced) updated.append(proposed);
                replaced = true;
            } else updated.append(entry);
        }
        if (!replaced) updated.append(proposed);
        rows = updated;
    }
    const QString moduleBefore = QString::fromUtf8(bytes(m_rulePath));
    const QString moduleAfter = ruleModule(rows);
    const QString configBefore = QString::fromUtf8(bytes(m_hyprlandConfig));
    QString configAfter = configBefore;
    if (index < 0 && !configAfter.contains(moduleInclude()) && !configAfter.contains("require(\"hypr.windowtuner\")") &&
        !configAfter.contains("require('hypr.windowtuner')")) {
        if (!configAfter.endsWith('\n')) configAfter.append('\n');
        configAfter.append("\n-- Load per-window rules saved by WindowTuner.\n" + moduleInclude() + "\n");
    }
    const auto diff = [](const QString &name, const QString &before, const QString &after) {
        if (before == after) return name + ": unchanged\n";
        return "--- " + name + " (current)\n+++ " + name + " (proposed)\n- " + before.split('\n').join("\n- ") +
            "\n+ " + after.split('\n').join("\n+ ") + "\n";
    };
    return {{"summary", summarize(effective)}, {"before", moduleBefore}, {"after", moduleAfter},
        {"moduleAfter", moduleAfter}, {"configAfter", configAfter},
        {"diff", diff("windowtuner.lua", moduleBefore, moduleAfter) + "\n" + diff("main Lua config", configBefore, configAfter)},
        {"errors", validateSettings(effective)}};
}

bool Backend::tryGeometry(const QVariantMap &settings, int seconds)
{
    if (trialActive()) { setStatus(tr("Revert the current trial first.")); return false; }
    refreshWindows();
    const auto window = selectedWindow();
    if (window.value("address").toString().isEmpty() || window.value("fullscreen").toInt() != 0 ||
        window.value("at").toList().size() != 2 || window.value("size").toList().size() != 2) {
        setStatus(tr("Select a non-fullscreen open window with known geometry.")); return false;
    }
    if (!validateSettings(settings).isEmpty()) { setStatus(validateSettings(settings).join("\n")); return false; }
    // Retain recovery information before the first side effect. Appearance and
    // workspace changes are deliberately excluded because their old overrides
    // cannot be reconstructed reliably from clients JSON.
    m_trialWindow = window;
    QVariantMap geometry;
    for (const auto &key : {"layout", "width", "height", "center", "setPosition", "x", "y"})
        if (settings.contains(key)) geometry.insert(key, settings.value(key));
    if (!run(m_hyprctl, {"eval", liveApplyCode(window, geometry)})) {
        revertTrial();
        setStatus(tr("Geometry trial failed; attempted to restore the original geometry."));
        return false;
    }
    m_trialTimer.start(qBound(1, seconds, 60) * 1000);
    emit safetyChanged();
    setStatus(tr("Geometry-only trial active; automatically reverting. No rule was saved."));
    return true;
}

bool Backend::revertTrial()
{
    if (!trialActive()) return true;
    const auto size = m_trialWindow.value("size").toList();
    const auto at = m_trialWindow.value("at").toList();
    const QVariantMap original{{"layout", m_trialWindow.value("floating").toBool() ? "floating" : "tiled"},
                               {"width", size[0]}, {"height", size[1]}, {"center", false},
                               {"setPosition", true}, {"x", at[0]}, {"y", at[1]}};
    if (!run(m_hyprctl, {"eval", liveApplyCode(m_trialWindow, original)})) {
        setStatus(tr("Could not revert trial. Keep this app open and retry Revert."));
        return false;
    }
    m_trialTimer.stop();
    m_trialWindow.clear();
    emit safetyChanged();
    setStatus(tr("Original layout and geometry restored."));
    return true;
}
