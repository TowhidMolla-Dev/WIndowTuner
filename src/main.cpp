#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <memory>

#include "backend.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    // Read-only commands work without a display, even in a desktop environment
    // that exports QT_QPA_PLATFORM=wayland.
    QString requested = "open";
    bool help = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--help" || arg == "-h" || arg == "--version" || arg == "-v") help = true;
        if (QStringList{"--config-directory", "--entry-point", "--hyprctl", "--settings-file"}.contains(arg)) { ++i; continue; }
        if (!arg.startsWith('-')) { requested = arg; break; }
    }
    if (help || requested != "open")
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("windowtuner");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("windowtuner");
    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
    QQuickStyle::setStyle("Material");

    QCommandLineParser parser;
    parser.setApplicationDescription("WindowTuner window rules and read-only diagnostics");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", "open (default), check, diagnostics, inspect, verify, export", "[command]");
    parser.addOption({"json", "Emit machine-readable JSON (the default for read-only commands)."});
    parser.addOption({"config-directory", "Rule data directory.", "path"});
    parser.addOption({"entry-point", "Main Lua configuration path.", "path"});
    parser.addOption({"hyprctl", "hyprctl executable path.", "path"});
    parser.addOption({"settings-file", "JSON settings object for verify.", "path"});
    parser.process(app);
    const auto args = parser.positionalArguments();
    const QString command = args.value(0, "open");
    if (args.size() > 1 || !QStringList{"open", "check", "diagnostics", "inspect", "verify", "export"}.contains(command)) {
        QTextStream(stderr) << "Unknown command. Use --help.\n"; return 1;
    }
    std::unique_ptr<Backend> instance;
    if (parser.isSet("config-directory") || parser.isSet("entry-point") || parser.isSet("hyprctl")) {
        const auto directory = parser.value("config-directory").isEmpty() ? QDir::homePath() + "/.config/hypr" : parser.value("config-directory");
        const auto entry = parser.value("entry-point").isEmpty() ? directory + "/hyprland.lua" : parser.value("entry-point");
        const auto ctl = parser.value("hyprctl").isEmpty() ? QStandardPaths::findExecutable("hyprctl") : parser.value("hyprctl");
        instance = std::make_unique<Backend>(directory, entry, ctl);
    } else instance = std::make_unique<Backend>();
    Backend &backend = *instance;
    if (command != "open") {
        if (command == "export" || command == "diagnostics") {
            QTextStream(stdout) << (command == "export" ? backend.exportRules() : backend.diagnosticReport());
            return 0;
        }
        QVariantMap report;
        bool ok = true;
        if (command == "check") {
            report = backend.healthReport();
            const auto errors = report.value("configErrors").toString().trimmed();
            ok = report.value("sessionReachable").toBool() && report.value("entryPointReadable").toBool() &&
                (errors.isEmpty() || errors == "[]" || errors == "No config errors found.");
        } else if (command == "inspect") report = backend.inspectWindow();
        else {
            QFile file(parser.value("settings-file"));
            if (!file.open(QIODevice::ReadOnly)) { QTextStream(stderr) << "verify requires --settings-file with a JSON object.\n"; return 1; }
            const auto doc = QJsonDocument::fromJson(file.readAll());
            if (!doc.isObject()) { QTextStream(stderr) << "Settings must be a JSON object.\n"; return 1; }
            report = backend.verifyDraft(backend.resolvePlacement(doc.object().toVariantMap()));
            ok = report.value("ok").toBool();
        }
        QTextStream(stdout) << QJsonDocument(QJsonObject::fromVariantMap(report)).toJson();
        if (!ok) QTextStream(stderr) << "WindowTuner " << command << " failed; see JSON output.\n";
        return ok ? 0 : 1;
    }
    Theme theme;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
