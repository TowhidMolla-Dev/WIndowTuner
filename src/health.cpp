#include "backend.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
QVariantMap query(const QString &program, const QStringList &args)
{
    if (program.isEmpty()) return {{"ok", false}, {"output", "Executable not found"}};
    QProcess process;
    process.start(program, args);
    const bool ok = process.waitForStarted(1500) && process.waitForFinished(5000) &&
                    process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return {{"ok", ok}, {"output", QString::fromUtf8(process.readAllStandardOutput()).trimmed()},
            {"stderr", QString::fromUtf8(process.readAllStandardError()).trimmed()}};
}
}

bool Backend::configureEntryPoint(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isAbsolute() || !info.isFile() || !info.isReadable() || info.suffix() != "lua" ||
        info.canonicalFilePath() == QFileInfo(m_rulePath).canonicalFilePath()) {
        setStatus(tr("Choose an existing, readable, absolute main .lua path, not the generated module."));
        return false;
    }
    if (trialActive() && !revertTrial()) return false;
    auto library = m_library; library.insert("entryPoint", info.canonicalFilePath());
    if (!writeLibrary(library)) return false;
    m_hyprlandConfig = info.canonicalFilePath();
    m_diskFingerprint = fingerprint();
    emit healthChanged();
    setStatus(tr("Entry point selected. Ensure the running Hyprland session actually loads it before saving."));
    return true;
}

QVariantMap Backend::healthReport() const
{
    auto version = query(m_hyprctl, {"-j", "version"});
    const auto versionObject = QJsonDocument::fromJson(version.value("output").toString().toUtf8()).object();
    const auto errors = query(m_hyprctl, {"configerrors"});
    QFile config(m_hyprlandConfig);
    const bool readable = config.open(QIODevice::ReadOnly);
    const auto contents = readable ? config.readAll() : QByteArray{};
    const auto probe = query(m_hyprctl, {"eval",
        "assert(type(hl.window_rule) == \"function\", \"missing hl.window_rule\"); "
        "assert(type(hl.dispatch) == \"function\", \"missing dispatch\"); "
        "assert(type(hl.dsp.window.move) == \"function\", \"missing move\"); "
        "assert(type(hl.dsp.window.resize) == \"function\", \"missing resize\"); "
        "assert(type(hl.dsp.window.set_prop) == \"function\", \"missing set_prop\")"});
    const bool apiVerified = probe.value("ok").toBool() && probe.value("output").toString() == "ok";
    const QString apiStatus = apiVerified ? tr("Verified: Lua rule and dispatcher functions exist") :
        probe.value("output").toString().contains("missing") ? tr("Unsupported: required Lua functions missing") :
        tr("Unverified: no successful capability response");
    return {{"version", versionObject.value("tag").toString(versionObject.value("version").toString("unknown"))},
        {"sessionReachable", version.value("ok").toBool() && !versionObject.isEmpty()},
        {"entryPoint", m_hyprlandConfig}, {"entryPointReadable", readable},
        {"module", m_rulePath}, {"moduleExists", QFile::exists(m_rulePath)},
        {"includePresent", contents.contains("hypr.windowtuner") || contents.contains(m_rulePath.toUtf8())},
        {"configErrors", errors.value("ok").toBool() ? errors.value("output") : QVariant(tr("Could not query config errors"))},
        {"externalChanges", fingerprint() != m_diskFingerprint}, {"lastSuccessfulSave", m_lastSuccessfulSave},
        {"capabilityStatus", apiStatus}, {"apiVerified", apiVerified},
        {"coverage", tr("Function availability is not proof of every property or dispatcher argument. Use Verify draft for installed-parser validation. The selected file is not guaranteed to be the running session's entry point.")}};
}

QVariantMap Backend::verifyDraft(const QVariantMap &settings) const
{
    const auto problems = validateSettings(settings);
    if (!problems.isEmpty()) return {{"status", "invalid"}, {"ok", false}, {"details", problems.join("\n")}};
    const QString program = QStandardPaths::findExecutable("Hyprland");
    if (program.isEmpty()) return {{"status", "unverified"}, {"ok", false}, {"details", tr("Hyprland executable not found.")}};
    QTemporaryDir dir;
    if (!dir.isValid()) return {{"ok", false}, {"status", "unverified"}, {"details", tr("Could not create isolated validation directory.")}};
    auto target = selectedWindow();
    if (target.value("class").toString().isEmpty()) target.insert("class", "windowtuner-validation");
    QFile config(dir.filePath("validation.lua"));
    if (!config.open(QIODevice::WriteOnly)) return {{"ok", false}, {"status", "unverified"}, {"details", tr("Could not write validation fixture.")}};
    QString constructors = liveApplyCode({{"address", "0x0"}}, settings);
    constructors.replace("hl.dispatch(", "local _windowtuner_dispatcher = (");
    // Dispatcher constructors validate arguments but are never dispatched.
    const auto match = settings.value("match").toMap();
    const QString title = match.value("title", settings.value("matchTitle", true).toBool() ? target.value("title") : QVariant("")).toString();
    const QJsonArray rows{QJsonObject{{"class", target.value("class").toString()}, {"title", title},
        {"match", QJsonObject::fromVariantMap(settings.value("match").toMap())}, {"settings", QJsonObject::fromVariantMap(settings)}}};
    const auto module = ruleModule(rows);
    if (module.isEmpty()) return {{"ok", false}, {"status", "unverified"}, {"details", tr("Bundled monitor runtime is unavailable.")}};
    config.write((module + "\n" + constructors + "\n").toUtf8());
    config.close();
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_CONFIG_HOME", dir.path());
    env.insert("XDG_STATE_HOME", dir.path());
    env.insert("XDG_CACHE_HOME", dir.path());
    env.remove("HYPRLAND_INSTANCE_SIGNATURE");
    QProcess process;
    process.setProcessEnvironment(env);
    process.start(program, {"--verify-config", "-c", config.fileName()});
    const bool finished = process.waitForStarted(1500) && process.waitForFinished(10000);
    const auto output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
    const bool ok = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return {{"ok", ok}, {"status", ok ? "verified-config-only" : finished ? "rejected" : "unverified"},
        {"details", output.left(24000)}, {"coverage", tr("Isolated installed-parser validation only; does not reload your session or prove runtime behavior.")}};
}

QString Backend::diagnosticReport() const
{
    // Export an allowlist, not a redacted raw dump. Window titles, app classes,
    // rule bodies, absolute paths, compositor errors and environment variables
    // never enter the shareable report.
    const auto health = healthReport();
    QJsonObject safe{{"application", "WindowTuner"}, {"version", QCoreApplication::applicationVersion()},
        {"hyprlandVersion", health.value("version").toString()},
        {"sessionReachable", health.value("sessionReachable").toBool()},
        {"luaApiVerified", health.value("apiVerified").toBool()},
        {"configReadable", health.value("entryPointReadable").toBool()},
        {"includePresent", health.value("includePresent").toBool()},
        {"moduleExists", health.value("moduleExists").toBool()},
        {"externalChanges", health.value("externalChanges").toBool()},
        {"windowCount", m_windows.size()}, {"monitorCount", m_monitors.size()}, {"ruleCount", m_savedRules.size()},
        {"lastSuccessfulSave", m_lastSuccessfulSave},
        {"privacy", "No titles, class names, raw config errors, paths, environment, rule bodies or monitor identifiers included."}};
    return QString::fromUtf8(QJsonDocument(safe).toJson());
}

QVariantList Backend::auditCapabilities() const
{
    const QList<QPair<QString, QVariantMap>> groups{
        {tr("Floating geometry and monitor placement"), {{"layout", "floating"}, {"monitor", "DP-1"}, {"setPosition", true}, {"center", false}}},
        {tr("Tiled layout"), {{"layout", "tiled"}}},
        {tr("Size constraints"), {{"minSizeEnabled", true}, {"maxSizeEnabled", true}}},
        {tr("Opacity"), {{"opacityEnabled", true}}},
        {tr("Border and rounding"), {{"borderEnabled", true}, {"roundingEnabled", true}}},
        {tr("Blur and shadow"), {{"noBlur", true}, {"noShadow", true}}},
        {tr("Initial focus"), {{"noInitialFocus", true}}},
        {tr("Animations, dimming, VRR and persistent size"), {{"noAnim", true}, {"noDim", true}, {"noVrr", true}, {"persistentSize", true}}},
        {tr("Workspace and pinning"), {{"workspace", "2 silent"}, {"pin", true}}},
        {tr("Advanced match predicates"), {{"match", QVariantMap{{"class", "windowtuner-probe"}, {"title", "Demo.*"},
            {"workspace", "1"}, {"xwayland", false}, {"fullscreen", false}, {"group", false}, {"content", "none"}}}, {"titleMatchMode", "regex"}}}};
    QVariantList rows;
    for (const auto &group : groups) {
        auto settings = group.second;
        if (!settings.contains("layout")) settings.insert("layout", "keep");
        if (!settings.contains("match")) settings.insert("match", QVariantMap{{"class", "windowtuner-probe"}});
        settings.insert("matchTitle", false);
        const auto verdict = verifyDraft(settings);
        rows.append(QVariantMap{{"name", group.first}, {"status", verdict.value("status")},
            {"details", verdict.value("ok").toBool() ? tr("Accepted by the installed parser; runtime behavior is not proven.") : verdict.value("details")}});
    }
    return rows;
}
