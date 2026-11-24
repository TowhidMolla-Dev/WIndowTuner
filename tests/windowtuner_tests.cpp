#include <QFile>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QProcess>
#include <QScopeGuard>
#include <QtTest>
#include <functional>

#include "backend.h"
#include "theme.h"

struct DesktopFixture {
    QTemporaryDir dir;
    bool write(const QString &name, const QByteArray &data, bool executable = false)
    {
        QDir().mkpath(QFileInfo(dir.filePath(name)).absolutePath());
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size()) return false;
        file.close();
        return !executable || file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    bool initialize()
    {
        return dir.isValid() && write("hyprland.lua", "-- fixture config\n") &&
            write("clients.json", R"([{"address":"0x1","class":"foot","initialClass":"foot","title":"Work","initialTitle":"Work","workspace":{"name":"1"},"at":[10,20],"size":[800,600],"floating":true}])") &&
            write("hyprctl", "#!/bin/sh\nif [ \"$2\" = clients ]; then cat '" + dir.filePath("clients.json").toUtf8() +
                "'; elif [ \"$2\" = monitors ]; then echo '[{\"name\":\"DP-1\",\"width\":1920,\"height\":1080,\"scale\":1,\"reserved\":[0,40,0,0]}]'; "
                "elif [ \"$2\" = version ]; then echo '{\"tag\":\"fixture-lua\"}'; elif [ \"$1\" = configerrors ]; then echo '[]'; fi\nexit 0\n", true);
    }
};

static QQuickItem *visualItem(QQuickItem *parent, const QString &name)
{
    if (parent->objectName() == name) return parent;
    for (auto *child : parent->childItems())
        if (auto *found = visualItem(child, name)) return found;
    return nullptr;
}

class WindowTunerTests : public QObject {
    Q_OBJECT

private slots:
    void commandPalettePinsAndConfirmsChangingActions()
    {
        DesktopFixture fixture;
        QVERIFY(fixture.initialize());
        const QString evalLog = fixture.dir.filePath("eval.log");
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\n"
            "if [ \"$1\" = eval ]; then printf '%s\\n' \"$2\" >> '" + evalLog.toUtf8() + "'; echo ok; exit 0; fi\n"
            "if [ \"$2\" = clients ]; then cat '" + fixture.dir.filePath("clients.json").toUtf8() + "'; "
            "elif [ \"$2\" = monitors ]; then echo '[{\"name\":\"DP-1\",\"width\":1920,\"height\":1080,\"scale\":1}]'; "
            "elif [ \"$2\" = version ]; then echo '{\"tag\":\"fixture-lua\"}'; "
            "elif [ \"$1\" = configerrors ]; then echo '[]'; fi\nexit 0\n", true));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        backend.selectWindow(0);
        QVERIFY(backend.saveRule({{"layout", "floating"}, {"width", 700}, {"height", 500}}));
        QVERIFY(backend.canUndo());
        const QString undoName = backend.undoSummary();
        QVERIFY(!undoName.isEmpty());
        QVERIFY(backend.setPreference("pinnedCommands", QStringList{"window.refresh", "history.undo"}));
        QCOMPARE(backend.preference("pinnedCommands", {}).toStringList(),
                 QStringList({"window.refresh", "history.undo"}));
        QVERIFY(!backend.setPreference("pinnedCommands", QStringList{"Not a command id"}));
        QStringList tooMany;
        for (int i = 0; i < 25; ++i) tooMany << QString("command%1").arg(i);
        QVERIFY(!backend.setPreference("pinnedCommands", tooMany));
        QCOMPARE(backend.preference("pinnedCommands", {}).toStringList(),
                 QStringList({"window.refresh", "history.undo"}));

        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window && QTest::qWaitForWindowExposed(window));
        QVERIFY(QMetaObject::invokeMethod(window, "openTool", Q_ARG(QVariant, QString("palette"))));
        auto *palette = window->findChild<QObject *>("commandPalette");
        QVERIFY(palette);
        QTRY_VERIFY(palette->property("opened").toBool());
        QCOMPARE(palette->property("pinned").toStringList(), QStringList({"window.refresh", "history.undo"}));
        const auto commands = [&] { return palette->property("filteredCommands").value<QJSValue>().toVariant().toList(); };
        QTRY_VERIFY(commands().size() > 3);
        QCOMPARE(commands().first().toMap().value("id").toString(), QString("window.refresh"));
        QCOMPARE(commands().at(1).toMap().value("id").toString(), QString("history.undo"));
        QVERIFY(commands().first().toMap().value("pinned").toBool());

        const auto item = [&](const QString &name) { return visualItem(window->contentItem(), name); };
        auto *search = item("commandPaletteSearch");
        auto *list = item("commandPaletteList");
        QVERIFY(search && list);
        QVERIFY(search->setProperty("text", "undo"));
        QTRY_COMPARE(list->property("count").toInt(), 2);
        QCOMPARE(commands().first().toMap().value("id").toString(), QString("history.undo"));
        search->forceActiveFocus();
        QTRY_VERIFY(search->hasActiveFocus());
        QVERIFY(QMetaObject::invokeMethod(palette, "moveSelection", Q_ARG(QVariant, 1)));
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(palette, "moveSelection", Q_ARG(QVariant, -1)));
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        QVERIFY(search->setProperty("text", ""));
        QTRY_VERIFY(list->property("count").toInt() > 3);
        search->forceActiveFocus();
        QTRY_VERIFY(search->hasActiveFocus());
        QVERIFY(QMetaObject::invokeMethod(palette, "moveSelection", Q_ARG(QVariant, 1)));
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(palette, "moveSelection", Q_ARG(QVariant, -1)));
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(palette, "activateCurrent"));
        QTRY_VERIFY(!palette->property("opened").toBool());
        QCOMPARE(palette->property("lastAction").toString(), QString("window.refresh"));

        QVERIFY(QMetaObject::invokeMethod(window, "openTool", Q_ARG(QVariant, QString("palette"))));
        QTRY_VERIFY(palette->property("opened").toBool());
        const auto applyCommand = [&]() {
            for (const auto &entry : commands()) if (entry.toMap().value("id").toString() == "window.applyRule") return entry.toMap();
            return QVariantMap{};
        }();
        QVERIFY(applyCommand.value("enabled").toBool());
        QVERIFY(applyCommand.value("changes").toBool());
        QVERIFY(QMetaObject::invokeMethod(palette, "activateCommand", Q_ARG(QVariant, QString("window.applyRule"))));
        auto *confirmation = window->findChild<QObject *>("commandPaletteConfirmation");
        QVERIFY(confirmation);
        QTRY_VERIFY(confirmation->property("opened").toBool());
        auto *summary = item("commandPaletteConfirmSummary");
        QVERIFY(summary);
        QVERIFY(summary->property("text").toString().contains("Window size"));
        const qint64 evalBefore = QFileInfo(evalLog).size();
        QVERIFY(QMetaObject::invokeMethod(confirmation, "reject"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        QCOMPARE(QFileInfo(evalLog).size(), evalBefore);
        QVERIFY(QMetaObject::invokeMethod(palette, "activateCommand", Q_ARG(QVariant, QString("window.applyRule"))));
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
        QTRY_VERIFY(QFileInfo(evalLog).size() > evalBefore);
        QTRY_VERIFY(!palette->property("opened").toBool());
        QCOMPARE(palette->property("lastAction").toString(), QString("window.applyRule"));

        QVERIFY(QMetaObject::invokeMethod(window, "openTool", Q_ARG(QVariant, QString("palette"))));
        QTRY_VERIFY(palette->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(palette, "activateCommand", Q_ARG(QVariant, QString("history.undo"))));
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(summary->property("text").toString().contains(undoName));
        QVERIFY(QMetaObject::invokeMethod(confirmation, "reject"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        QVERIFY(backend.canUndo());
        QVERIFY(QMetaObject::invokeMethod(palette, "activateCommand", Q_ARG(QVariant, QString("history.undo"))));
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
        QTRY_VERIFY(!backend.canUndo());
        QTRY_VERIFY(!palette->property("opened").toBool());
        QCOMPARE(palette->property("lastAction").toString(), QString("history.undo"));

        QVERIFY(QMetaObject::invokeMethod(window, "openTool", Q_ARG(QVariant, QString("palette"))));
        QTRY_VERIFY(palette->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(palette, "togglePin", Q_ARG(QVariant, QString("tool.health"))));
        QCOMPARE(backend.preference("pinnedCommands", {}).toStringList(),
                 QStringList({"window.refresh", "history.undo", "tool.health"}));
        QVERIFY(QMetaObject::invokeMethod(palette, "movePinned", Q_ARG(QVariant, QString("tool.health")), Q_ARG(QVariant, -1)));
        QCOMPARE(backend.preference("pinnedCommands", {}).toStringList(),
                 QStringList({"window.refresh", "tool.health", "history.undo"}));
        QCOMPARE(commands().at(1).toMap().value("id").toString(), QString("tool.health"));
        QVERIFY(QMetaObject::invokeMethod(window, "navigate", Q_ARG(QVariant, 4)));
        QVERIFY(search->setProperty("text", "layout and size"));
        QTRY_COMPARE(list->property("count").toInt(), 1);
        search->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!palette->property("opened").toBool());
        QTRY_COMPARE(window->property("category").toInt(), 0);

        Backend reopened(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QCOMPARE(reopened.preference("pinnedCommands", {}).toStringList(),
                 QStringList({"window.refresh", "tool.health", "history.undo"}));
        QCOMPARE(warnings.size(), 0);
    }

    void encryptedMigrationAuthenticatesAndRequiresConfirmation()
    {
        DesktopFixture source, destination;
        QVERIFY(source.initialize() && destination.initialize());
        QVERIFY(source.write("clients.json", R"([{"address":"0x1","class":"org.test.MigrationSource","title":"Migration Window","monitor":0,"workspace":{"name":"1"},"at":[10,20],"size":[800,600],"floating":true}])"));
        QVERIFY(source.write("hyprctl", "#!/bin/sh\n"
            "if [ \"$2\" = clients ]; then cat '" + source.dir.filePath("clients.json").toUtf8() + "'; "
            "elif [ \"$2\" = monitors ]; then echo '[{\"id\":0,\"name\":\"DP-7\",\"description\":\"Source Display\",\"width\":1920,\"height\":1080,\"scale\":1,\"activeWorkspace\":{\"name\":\"1\"}}]'; "
            "elif [ \"$1\" = configerrors ]; then echo '[]'; fi\nexit 0\n", true));
        Backend sender(source.dir.path(), source.dir.filePath("hyprland.lua"), source.dir.filePath("hyprctl"));
        QVERIFY(sender.saveRule({{"layout", "floating"}, {"width", 800}, {"height", 600},
            {"launchCommand", "local-only-secret-example"}}));
        QVERIFY(sender.setWorkspacePlan("1", "Source Display", true));
        QVERIFY(sender.saveProfile("Travel"));
        QVERIFY(sender.captureScene("Work scene"));
        const QString passphrase = "test transfer passphrase";
        const auto bundle = sender.exportMigration(passphrase).value("bundle").toString();
        QVERIFY(bundle.startsWith("WTM1:"));
        QVERIFY(!bundle.contains("MigrationSource"));
        QVERIFY(sender.exportMigration(passphrase).value("bundle").toString() != bundle);

        Backend receiver(destination.dir.path(), destination.dir.filePath("hyprland.lua"), destination.dir.filePath("hyprctl"));
        QVERIFY(receiver.setPreference("onboarded", true));
        QVERIFY(receiver.saveRule({{"layout", "tiled"}}));
        const auto originalRules = receiver.exportRules();
        QVERIFY(receiver.previewMigration(bundle, "wrong credentials").contains("error"));
        auto envelope = QByteArray::fromBase64(bundle.mid(5).toLatin1());
        envelope[envelope.size() - 1] = char(envelope.back() ^ 1);
        const auto tampered = QString("WTM1:") + QString::fromLatin1(envelope.toBase64());
        QVERIFY(receiver.previewMigration(tampered, passphrase).contains("error"));
        const auto review = receiver.previewMigration(bundle, passphrase);
        QCOMPARE(review.value("missingApps").toStringList(), QStringList{"org.test.MigrationSource"});
        QCOMPARE(review.value("missingDisplays").toStringList(), QStringList{"Source Display"});
        QVERIFY(!review.value("after").toString().contains("local-only-secret-example"));
        QVERIFY(review.value("before").toString().contains("foot"));
        QVERIFY(!receiver.applyMigration(tampered, passphrase, review.value("token").toString()));
        QCOMPARE(receiver.exportRules(), originalRules);

        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &receiver);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window && QTest::qWaitForWindowExposed(window));
        QVERIFY(QMetaObject::invokeMethod(window, "openTool", Q_ARG(QVariant, QString("migration"))));
        auto *dialog = window->findChild<QObject *>("migrationDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        const auto item = [&](const QString &name) { return visualItem(window->contentItem(), name); };
        auto *input = item("migrationBundle"), *password = item("migrationPassphrase"),
             *preview = item("previewMigrationAction"), *confirm = item("reviewMigrationAction");
        QVERIFY(input && password && preview && confirm);
        QVERIFY(input->setProperty("text", bundle));
        QVERIFY(password->setProperty("text", passphrase));
        QVERIFY(QMetaObject::invokeMethod(preview, "clicked"));
        QVERIFY(item("migrationWarnings")->property("text").toString().contains("Source Display"));
        QVERIFY(item("migrationComparison")->property("text").toString().contains("Travel"));
        QVERIFY(confirm->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(confirm, "clicked"));
        auto *confirmation = window->findChild<QObject *>("confirmMigrationDialog");
        QVERIFY(confirmation);
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(confirmation, "reject"));
        QTRY_VERIFY(!confirmation->property("visible").toBool());
        QCOMPARE(receiver.exportRules(), originalRules);
        QVERIFY(receiver.profiles().isEmpty());
        QVERIFY(receiver.scenes().isEmpty());
        QVERIFY(receiver.workspacePlan().first().toMap().value("monitor").toString().isEmpty());

        QVERIFY(QMetaObject::invokeMethod(confirm, "clicked"));
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
        QTRY_COMPARE(receiver.scenes().size(), 1);
        QCOMPARE(receiver.profiles().first().toMap().value("name").toString(), QString("Travel"));
        QCOMPARE(receiver.workspacePlan().first().toMap().value("monitor").toString(), QString("Source Display"));
        QVERIFY(receiver.preference("onboarded", false).toBool());
        QVERIFY(!receiver.previewMigration(bundle, passphrase).value("changed").toBool());
        QVERIFY(receiver.undo());
        QCOMPARE(receiver.exportRules(), originalRules);
        QVERIFY(receiver.scenes().isEmpty());
        QVERIFY(receiver.profiles().isEmpty());
        QVERIFY(receiver.redo());
        QCOMPARE(receiver.scenes().size(), 1);
        QCOMPARE(receiver.profiles().size(), 1);
        QCOMPARE(warnings.size(), 0);
    }

    void ruleRequestsOnlySaveAfterNormalConfirmation()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.setPreference("onboarded", true));
        QVERIFY(backend.interpretRuleRequest("open this app tiled at 800x600").contains("error"));
        QVERIFY(backend.interpretRuleRequest("open this app floating at 0x600").contains("error"));
        QVERIFY(backend.interpretRuleRequest("open this app on workspace 2 on workspace 3").contains("error"));
        QCOMPARE(backend.interpretRuleRequest("open this app tiled on workspace 2")
            .value("settings").toMap().value("layout").toString(), QString("tiled"));

        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window && QTest::qWaitForWindowExposed(window));
        window->resize(1280, 900);
        QVERIFY(QMetaObject::invokeMethod(window, "openTool", Q_ARG(QVariant, QString("describeRule"))));
        auto *dialog = window->findChild<QObject *>("ruleRequestDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        const auto item = [&](const QString &name) { return visualItem(window->contentItem(), name); };
        auto *input = item("ruleRequestInput"), *parse = item("interpretRuleRequest"), *stage = item("stageRuleRequest");
        auto *result = item("ruleRequestResult");
        QVERIFY(input && parse && stage && result);
        QVERIFY(input->setProperty("text", "open this app on workspace 3 and run a command"));
        QVERIFY(QMetaObject::invokeMethod(parse, "clicked"));
        QVERIFY(result->property("text").toString().contains("Unsupported wording"));
        QVERIFY(!stage->isEnabled());
        QVERIFY(backend.savedRules().isEmpty());

        QVERIFY(input->setProperty("text", "Open this app floating at 900×650 centered on workspace 3."));
        QVERIFY(QMetaObject::invokeMethod(parse, "clicked"));
        QVERIFY(result->property("text").toString().contains("Match class: foot"));
        QVERIFY(result->property("text").toString().contains("Workspace: 3"));
        QVERIFY(stage->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(stage, "clicked"));
        QTRY_VERIFY(window->property("dirty").toBool());
        QTRY_VERIFY(item("previewWindowShape") && item("previewWindowShape")->isVisible());
        const auto geometry = item("previewWindowShape")->property("geometry").value<QJSValue>().toVariant().toMap();
        QCOMPARE(geometry.value("width").toInt(), 900);
        QCOMPARE(geometry.value("height").toInt(), 650);
        auto *width = item("widthField");
        QVERIFY(width && width->setProperty("value", 920));
        QVERIFY(backend.savedRules().isEmpty());

        auto *review = window->findChild<QObject *>("saveReview");
        QVERIFY(review);
        QVERIFY(QMetaObject::invokeMethod(window, "requestSave"));
        QTRY_VERIFY(review->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(review, "reject"));
        QTRY_VERIFY(!review->property("visible").toBool());
        QVERIFY(backend.savedRules().isEmpty());
        QVERIFY(!QFileInfo::exists(fixture.dir.filePath("windowtuner.json")));
        QVERIFY(QMetaObject::invokeMethod(window, "requestSave"));
        QTRY_VERIFY(review->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(review, "accept"));
        QTRY_COMPARE(backend.savedRules().size(), 1);
        const auto saved = backend.savedRules().first().toMap().value("settings").toMap();
        QCOMPARE(saved.value("width").toInt(), 920);
        QCOMPARE(saved.value("workspace").toString(), QString("3"));
        QCOMPARE(warnings.size(), 0);
    }

    void geometrySuggestionsAreOptInDismissibleAndDraftOnly()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.setPreference("onboarded", true));
        QVERIFY(backend.saveRule({{"layout", "floating"}, {"width", 800}, {"height", 600},
            {"center", false}, {"setPosition", true}, {"x", 10}, {"y", 20}}));
        const auto rules = backend.exportRules();
        const auto geometry = [&](int width, int x) {
            const auto json = QString(R"([{"address":"0x1","class":"foot","title":"Work","workspace":{"name":"1"},"at":[%1,20],"size":[%2,600],"floating":true}])")
                .arg(x).arg(width).toUtf8();
            QVERIFY(fixture.write("clients.json", json));
            QVERIFY(backend.refreshWindows());
        };
        QVERIFY(!backend.geometrySuggestionsEnabled());
        geometry(880, 70);
        geometry(960, 130);
        QVERIFY(backend.geometrySuggestion().isEmpty());

        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window && QTest::qWaitForWindowExposed(window));
        auto *banner = visualItem(window->contentItem(), "geometrySuggestionBanner");
        QVERIFY(banner && !banner->isVisible());

        QVERIFY(backend.setGeometrySuggestionsEnabled(true));
        geometry(800, 10);
        geometry(880, 70);
        geometry(880, 70); // Polling the same state is not a second change.
        QVERIFY(!banner->isVisible());
        geometry(960, 130);
        QTRY_VERIFY(banner->isVisible());
        QCOMPARE(backend.exportRules(), rules);
        auto *dismiss = visualItem(window->contentItem(), "dismissGeometrySuggestion");
        QVERIFY(dismiss && QMetaObject::invokeMethod(dismiss, "clicked"));
        QTRY_VERIFY(!banner->isVisible());
        geometry(1040, 200);
        QVERIFY(!banner->isVisible());

        QVERIFY(backend.setGeometrySuggestionsEnabled(false));
        geometry(800, 10);
        geometry(880, 70);
        geometry(960, 130);
        QVERIFY(!banner->isVisible());
        QCOMPARE(backend.exportRules(), rules);

        QVERIFY(backend.setGeometrySuggestionsEnabled(true));
        geometry(800, 10);
        geometry(880, 70);
        geometry(960, 130);
        QTRY_VERIFY(banner->isVisible());
        auto *review = visualItem(window->contentItem(), "reviewGeometrySuggestion");
        QVERIFY(review && review->isEnabled() && QMetaObject::invokeMethod(review, "clicked"));
        QTRY_VERIFY(window->property("dirty").toBool());
        QCOMPARE(visualItem(window->contentItem(), "widthField")->property("value").toInt(), 960);
        QCOMPARE(backend.exportRules(), rules);
        QCOMPARE(warnings.size(), 0);
    }

    void distinguishesAppHintsFromObservedWindowMatches()
    {
        const QVariantList apps{
            QVariantMap{{"name", "Terminal"}, {"class", "foot"}, {"inferred", false}},
            QVariantMap{{"name", "Browser"}, {"class", "browser"}, {"inferred", false}},
            QVariantMap{{"name", "Filename guess"}, {"class", "foot"}, {"inferred", true}}
        };
        QVariantMap match{{"class", "foot"}};
        auto report = Backend::simulateAppHints({{"match", match}}, apps);
        QCOMPARE(report.value("matches").toList().size(), 1);
        QCOMPARE(report.value("unaffected").toList().size(), 1);
        QCOMPARE(report.value("uncertain").toList().size(), 1);
        QVERIFY(report.value("matches").toList().first().toMap().value("reason").toString().contains("StartupWMClass"));

        match.insert("title", "Work");
        report = Backend::simulateAppHints({{"match", match}}, apps);
        QCOMPARE(report.value("matches").toList().size(), 0);
        QCOMPARE(report.value("uncertain").toList().size(), 2);

        match.remove("title");
        report = Backend::simulateAppHints({{"match", match}, {"excludeTitle", "Private.*"}}, apps);
        QCOMPARE(report.value("matches").toList().size(), 0);
        QVERIFY(report.value("uncertain").toList().first().toMap().value("reason").toString().contains("exclusion"));
    }

    void monitorDisconnectReconnectRuntime()
    {
        const auto lua = QStandardPaths::findExecutable("lua");
        QVERIFY2(!lua.isEmpty(), "Install Lua to run the production monitor-runtime regression tests.");
        QProcess process;
        process.start(lua, {QFINDTESTDATA("monitor_recovery.lua"), QFINDTESTDATA("../src/monitor_recovery.lua")});
        QVERIFY(process.waitForFinished(10000));
        const auto output = process.readAllStandardOutput() + process.readAllStandardError();
        QVERIFY2(process.exitCode() == 0, output.constData());
        QVERIFY(output.contains("15 regression scenarios passed"));
    }
    void generatedMonitorRuntimeIsReviewedAndValidated()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        const QVariantMap placement{{"enabled", true}, {"runtimeRecovery", true}, {"monitor", "DP-1"},
            {"fallback", "eDP-1"}, {"unit", "percent"}, {"width", 50}, {"height", 50}};
        const auto settings = backend.resolvePlacement({{"matchTitle", false}, {"placement", placement}});
        const auto review = backend.reviewChanges(settings);
        QVERIFY(review.value("moduleAfter").toString().contains("install_windowtuner_recovery"));
        const auto verdict = backend.verifyDraft(settings);
        QVERIFY2(verdict.value("ok").toBool(), verdict.value("details").toString().toUtf8().constData());
        QVERIFY(backend.saveRule(settings));
        QFile module(fixture.dir.filePath("windowtuner.lua")); QVERIFY(module.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(module.readAll()), review.value("moduleAfter").toString());
        module.close();
        QVERIFY(backend.setRuleEnabled(0, false));
        QVERIFY(module.open(QIODevice::ReadOnly));
        const auto disabled = module.readAll();
        QVERIFY(!disabled.contains("install_windowtuner_recovery"));
        QVERIFY(disabled.contains("__windowtuner_placement_runtime.stop()"));
    }
    void appSpaceRuntimeScenarios()
    {
        const auto lua = QStandardPaths::findExecutable("lua");
        QVERIFY2(!lua.isEmpty(), "Install Lua to run the production AppSpace runtime tests.");
        QProcess process;
        process.start(lua, {QFINDTESTDATA("appspace.lua"), QFINDTESTDATA("../src/appspace_runtime.lua")});
        QVERIFY(process.waitForFinished(15000));
        const auto output = process.readAllStandardOutput() + process.readAllStandardError();
        QVERIFY2(process.exitCode() == 0, output.constData());
        QVERIFY(output.contains("18 regression scenarios passed"));
    }
    void derivesApplicationClassesFromDesktopEntries()
    {
        // A declared class is what Hyprland will compare against.
        QCOMPARE(Backend::deriveApplicationClass("org.example.Editor", "org.example.Editor", "/usr/bin/editor %U", "chromium"),
                 QString("org.example.Editor"));
        QCOMPARE(Backend::deriveApplicationClass("steam", "custom-class", "steam -applaunch 1", "chromium"),
                 QString("custom-class"));
        QCOMPARE(Backend::deriveApplicationClass("steam-1234", "", "steam://rungameid/1234", "chromium"),
                 QString("steam_app_1234"));
        QCOMPARE(Backend::deriveApplicationClass("weird", "", "omarchy-launch-tui htop", "chromium"),
                 QString("org.omarchy.htop"));
        QCOMPARE(Backend::deriveApplicationClass("weird", "", "foot --app-id=org.example.Term", "chromium"),
                 QString("org.example.Term"));
        QCOMPARE(Backend::deriveApplicationClass("chromium-webapp-abc", "",
                 "omarchy-launch-webapp https://chat.example.com/", "chromium"),
                 QString("chromium-chat.example.com__-Default"));
        // A broken declaration must not become a match: fall back to the id.
        QCOMPARE(Backend::deriveApplicationClass("org.example.Tool", "has space", "/usr/bin/tool %f", "chromium"),
                 QString("org.example.Tool"));
        QCOMPARE(Backend::desktopExecCommand("/usr/bin/player --play %U --title=%f"),
                 QString("/usr/bin/player --play --title="));
        QCOMPARE(Backend::desktopLaunchTarget("org.example.Editor", "/usr/bin/editor"),
                 QString("org.example.Editor.desktop"));
        QCOMPARE(Backend::desktopLaunchTarget("", "/usr/bin/editor %U"), QString("/usr/bin/editor"));
    }
    void importsAppSpacePlacementDisplaysAndAliases()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        const QString source = fixture.dir.filePath("appspace.json");
        QVERIFY(fixture.write("appspace.json", R"({
            "version": 6,
            "rules": [
              {"class":"tmail","workspace":"2","silent":true,"label":"TMail","verified":true,
               "autostart":true,"command":"tmail.desktop","startupOnly":true,"startupWindow":45},
              {"class":"steam_app_1","workspace":"","silent":false,"label":"Game","verified":false,
               "autostart":false,"command":"/usr/bin/game","startupOnly":false,"startupWindow":20}
            ],
            "workspaces": {"2": {"monitor": "WAM WD215I09", "persistent": true}},
            "monitors": {"WAM WD215I09": {"mode": "1920x1080@75.00", "x": 0, "y": 0, "scale": 1, "transform": 0}},
            "aliases": {"steam_app_1": "com.example.Game"}
        })"));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));

        QVERIFY(backend.appSpacePath().endsWith("/omarchy/appspace/rules.json"));
        const auto preview = backend.appSpaceImportPreview(source);
        QVERIFY2(preview.value("ok").toBool(), qPrintable(preview.value("error").toString()));
        QCOMPARE(preview.value("added").toInt(), 2);
        QCOMPARE(preview.value("updated").toInt(), 0);
        QCOMPARE(preview.value("workspaces").toInt(), 1);
        QCOMPARE(preview.value("monitors").toInt(), 1);
        QVERIFY(preview.value("sample").toList().size() == 2);
        // Preview must not write anything.
        QVERIFY(!QFile::exists(fixture.dir.filePath("windowtuner.json")));

        QVERIFY2(backend.importAppSpace(source), qPrintable(backend.status()));
        QCOMPARE(backend.savedRules().size(), 2);
        const auto settingsFor = [&](const QString &klass) {
            for (const auto &entry : backend.savedRules())
                if (entry.toMap().value("class").toString() == klass)
                    return entry.toMap().value("settings").toMap();
            return QVariantMap{};
        };
        const auto tmail = settingsFor("tmail");
        QCOMPARE(tmail.value("workspace").toString(), QString("2"));
        QVERIFY(tmail.value("noInitialFocus").toBool());
        QVERIFY(tmail.value("autostart").toBool());
        QCOMPARE(tmail.value("launchDesktop").toString(), QString("tmail.desktop"));
        QCOMPARE(tmail.value("launchCommand").toString(), QString("gtk-launch tmail.desktop"));
        QVERIFY(tmail.value("startupOnly").toBool());
        QCOMPARE(tmail.value("startupWindow").toInt(), 45);
        // The alias replaces the class Steam reported at first launch.
        const auto game = settingsFor("com.example.Game");
        QVERIFY(!game.isEmpty());
        QCOMPARE(game.value("launchDesktop").toString(), QString());
        QCOMPARE(game.value("launchCommand").toString(), QString("/usr/bin/game"));
        QCOMPARE(game.value("workspace").toString(), QString());

        const auto plan = backend.workspacePlan();
        QCOMPARE(plan.at(1).toMap().value("monitor").toString(), QString("WAM WD215I09"));
        QVERIFY(plan.at(1).toMap().value("persistent").toBool());
        QVERIFY(plan.at(1).toMap().value("apps").toStringList().contains("tmail"));

        QFile module(fixture.dir.filePath("windowtuner.lua")); QVERIFY(module.open(QIODevice::ReadOnly));
        const auto lua = QString::fromUtf8(module.readAll());
        module.close();
        QVERIFY(lua.contains("install_windowtuner_appspace"));
        QVERIFY(lua.contains("__windowtuner_ws"));
        QVERIFY(lua.contains("WAM WD215I09"));
        QVERIFY(lua.contains("uwsm-app -- tmail.desktop"));
        QVERIFY(lua.contains("\"startup\""));
        QFile library(fixture.dir.filePath("windowtuner-library.json")); QVERIFY(library.open(QIODevice::ReadOnly));
        const auto libraryJson = QJsonDocument::fromJson(library.readAll()).object();
        QCOMPARE(libraryJson.value("workspaces").toObject().value("2").toObject().value("monitor").toString(),
                 QString("WAM WD215I09"));
        QCOMPARE(libraryJson.value("monitors").toObject().value("WAM WD215I09").toObject().value("mode").toString(),
                 QString("1920x1080@75.00"));

        // Importing again updates instead of duplicating.
        QVERIFY(backend.importAppSpace(source));
        QCOMPARE(backend.savedRules().size(), 2);
        QCOMPARE(backend.appSpaceImportPreview(source).value("updated").toInt(), 2);

        // The generated module must be accepted by the installed Hyprland Lua
        // parser with the runtime, workspace pins and startup data inside it.
        const auto program = QStandardPaths::findExecutable("Hyprland");
        if (!program.isEmpty()) {
            QTemporaryDir isolate;
            QVERIFY(isolate.isValid());
            QFile regenerated(fixture.dir.filePath("windowtuner.lua"));
            QVERIFY(regenerated.open(QIODevice::ReadOnly));
            QVERIFY(fixture.write("validation.lua", regenerated.readAll()));
            auto env = QProcessEnvironment::systemEnvironment();
            env.insert("XDG_CONFIG_HOME", isolate.path());
            env.insert("XDG_STATE_HOME", isolate.path());
            env.insert("XDG_CACHE_HOME", isolate.path());
            env.remove("HYPRLAND_INSTANCE_SIGNATURE");
            QProcess process;
            process.setProcessEnvironment(env);
            process.start(program, {"--verify-config", "-c", fixture.dir.filePath("validation.lua")});
            QVERIFY(process.waitForStarted(2000) && process.waitForFinished(15000));
            const auto output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
            QVERIFY2(process.exitCode() == 0 && process.exitStatus() == QProcess::NormalExit,
                     output.toUtf8().constData());
        }
    }
    void namedScenesCapturePreviewAndApplyOnlyMatchedWindows()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        QVERIFY(fixture.write("clients.json",
            R"([{"address":"0xabc","class":"org.example.Editor","initialClass":"org.example.Editor","title":"PRIVATE-SCENE-TITLE","initialTitle":"PRIVATE-SCENE-TITLE","workspace":{"name":"2"},"monitor":0,"at":[250,140],"size":[900,700],"floating":true,"fullscreen":0}])"));
        QVERIFY(fixture.write("hyprctl",
            "#!/bin/sh\n"
            "if [ \"$2\" = clients ]; then cat '" + fixture.dir.filePath("clients.json").toUtf8() + "'; "
            "elif [ \"$2\" = monitors ]; then echo '[{\"id\":0,\"name\":\"DP-1\",\"description\":\"Monitor A\",\"x\":0,\"y\":0,\"width\":1920,\"height\":1080,\"scale\":1,\"activeWorkspace\":{\"id\":2,\"name\":\"2\"}}]'; "
            "elif [ \"$1\" = eval ]; then printf '%s\\n' \"$2\" >> '" + fixture.dir.filePath("eval.log").toUtf8() + "'; echo ok; fi\n"
            "exit 0\n", true));

        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        bool captured = false;
        QVERIFY(QMetaObject::invokeMethod(&backend, "captureScene", Q_RETURN_ARG(bool, captured),
                                          Q_ARG(QString, "Work")));
        QVERIFY(captured);
        const QVariantList scenes = backend.property("scenes").toList();
        QCOMPARE(scenes.size(), 1);
        const QString sceneId = scenes.first().toMap().value("id").toString();
        QVERIFY(!sceneId.isEmpty());
        QVERIFY(!scenes.first().toMap().contains("windows"));
        QVERIFY(QMetaObject::invokeMethod(&backend, "captureScene", Q_RETURN_ARG(bool, captured),
                                          Q_ARG(QString, "work")));
        QVERIFY(!captured);
        QVERIFY(QMetaObject::invokeMethod(&backend, "captureScene", Q_RETURN_ARG(bool, captured),
                                          Q_ARG(QString, "   ")));
        QVERIFY(!captured);

        QVariantMap preview;
        QVERIFY(QMetaObject::invokeMethod(&backend, "previewScene", Q_RETURN_ARG(QVariantMap, preview),
                                          Q_ARG(QString, sceneId)));
        QCOMPARE(preview.value("matched").toInt(), 1);
        QCOMPARE(preview.value("missing").toInt(), 0);
        const QVariantMap previewItem = preview.value("items").toList().first().toMap();
        QCOMPARE(previewItem.value("currentMonitor").toString(), QString("DP-1"));
        QCOMPARE(previewItem.value("currentWorkspace").toString(), QString("2"));
        QCOMPARE(previewItem.value("targetMonitor").toString(), QString("Monitor A"));
        QCOMPARE(previewItem.value("targetWorkspace").toString(), QString("2"));
        QCOMPARE(previewItem.value("width").toDouble(), 900.0 / 1920.0);
        QCOMPARE(previewItem.value("height").toDouble(), 700.0 / 1080.0);
        QVERIFY(!previewItem.contains("address"));
        QVERIFY(!previewItem.contains("title"));
        QVERIFY(!QFileInfo::exists(fixture.dir.filePath("eval.log")));
        QVERIFY(!backend.applyRehearsal(sceneId, false, {"stale-token"}));
        QVERIFY(!QFileInfo::exists(fixture.dir.filePath("eval.log")));

        bool opened = false;
        QVERIFY(QMetaObject::invokeMethod(&backend, "openScene", Q_RETURN_ARG(bool, opened),
                                          Q_ARG(QString, sceneId)));
        QVERIFY(opened);
        QFile log(fixture.dir.filePath("eval.log"));
        QVERIFY(log.open(QIODevice::ReadOnly));
        const QByteArray applied = log.readAll();
        QVERIFY(applied.contains("address:0xabc"));
        QVERIFY(applied.contains("workspace = \"2\""));
        QVERIFY(applied.contains("x = 900"));
        QVERIFY(applied.contains("y = 700"));
        log.close();

        QFile library(fixture.dir.filePath("windowtuner-library.json"));
        QVERIFY(library.open(QIODevice::ReadOnly));
        const QByteArray stored = library.readAll();
        QVERIFY(!stored.contains("PRIVATE-SCENE-TITLE"));
        QVERIFY(!stored.contains("0xabc"));
        Backend reopened(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QCOMPARE(reopened.property("scenes").toList().size(), 1);

        QVERIFY(fixture.write("clients.json", "[]"));
        backend.refreshWindows();
        QVERIFY(QMetaObject::invokeMethod(&backend, "previewScene", Q_RETURN_ARG(QVariantMap, preview),
                                          Q_ARG(QString, sceneId)));
        QCOMPARE(preview.value("matched").toInt(), 0);
        QCOMPARE(preview.value("missing").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(&backend, "openScene", Q_RETURN_ARG(bool, opened),
                                          Q_ARG(QString, sceneId)));
        QVERIFY(!opened);
        QVERIFY(log.open(QIODevice::ReadOnly));
        QCOMPARE(log.readAll(), applied);
        QVERIFY(QMetaObject::invokeMethod(&backend, "captureScene", Q_RETURN_ARG(bool, captured),
                                          Q_ARG(QString, "Empty")));
        QVERIFY(!captured);
        bool removed = false;
        QVERIFY(QMetaObject::invokeMethod(&backend, "removeScene", Q_RETURN_ARG(bool, removed),
                                          Q_ARG(QString, sceneId)));
        QVERIFY(removed);
        QCOMPARE(backend.property("scenes").toList().size(), 0);
    }
    void sessionSnapshotsAreOptInDebouncedAndManuallyRestored()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        const QByteArray firstClient =
            R"([{"address":"0xfeed","class":"org.example.Editor","initialClass":"org.example.Editor","title":"SESSION-PRIVATE-TITLE","initialTitle":"SESSION-PRIVATE-TITLE","workspace":{"name":"2"},"monitor":0,"at":[250,140],"size":[900,700],"floating":true,"fullscreen":0}])";
        QVERIFY(fixture.write("clients.json", firstClient));
        QVERIFY(fixture.write("hyprctl",
            "#!/bin/sh\n"
            "if [ \"$2\" = clients ]; then cat '" + fixture.dir.filePath("clients.json").toUtf8() + "'; "
            "elif [ \"$2\" = monitors ]; then echo '[{\"id\":0,\"name\":\"DP-1\",\"description\":\"Monitor A\",\"x\":0,\"y\":0,\"width\":1920,\"height\":1080,\"scale\":1,\"activeWorkspace\":{\"id\":2,\"name\":\"2\"}}]'; "
            "elif [ \"$1\" = eval ]; then printf '%s\\n' \"$2\" >> '" + fixture.dir.filePath("eval.log").toUtf8() + "'; echo ok; fi\n"
            "exit 0\n", true));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(!backend.property("sessionSnapshotsEnabled").toBool());
        QCOMPARE(backend.property("sessionSnapshots").toList().size(), 0);
        QVERIFY(backend.setSessionSnapshotsEnabled(true));
        QVERIFY(backend.property("sessionSnapshotsEnabled").toBool());
        QCOMPARE(backend.property("sessionSnapshots").toList().size(), 1);
        const QString oldestId = backend.property("sessionSnapshots").toList().first().toMap().value("id").toString();

        QVERIFY(fixture.write("clients.json",
            R"([{"address":"0xfeed","class":"org.example.Editor","initialClass":"org.example.Editor","title":"SESSION-PRIVATE-TITLE","initialTitle":"SESSION-PRIVATE-TITLE","workspace":{"name":"2"},"monitor":0,"at":[500,240],"size":[800,600],"floating":true,"fullscreen":0}])"));
        QVERIFY(QMetaObject::invokeMethod(&backend, "handleHyprlandEvent"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.property("sessionSnapshots").toList().size(), 2, 5000);
        QVariantMap preview;
        QVERIFY(QMetaObject::invokeMethod(&backend, "previewSessionSnapshot", Q_RETURN_ARG(QVariantMap, preview),
                                          Q_ARG(QString, oldestId)));
        QCOMPARE(preview.value("matched").toInt(), 1);
        QCOMPARE(preview.value("missing").toInt(), 0);
        QVERIFY(!preview.value("items").toList().first().toMap().contains("address"));

        const auto row = preview.value("items").toList().first().toMap();
        QCOMPARE(row.value("targetWorkspace").toString(), QString("2"));
        QVERIFY(!row.value("before").toMap().isEmpty());
        QVERIFY(row.value("before") != row.value("after"));
        QVERIFY(backend.applyRehearsal(oldestId, true, {row.value("token").toString()}));
        QFile log(fixture.dir.filePath("eval.log"));
        QVERIFY(log.open(QIODevice::ReadOnly));
        const QByteArray commands = log.readAll();
        QVERIFY(commands.contains("address:0xfeed"));
        QVERIFY(commands.contains("workspace = \"2\""));
        QFile library(fixture.dir.filePath("windowtuner-library.json"));
        QVERIFY(library.open(QIODevice::ReadOnly));
        const QByteArray stored = library.readAll();
        QVERIFY(!stored.contains("SESSION-PRIVATE-TITLE"));
        QVERIFY(!stored.contains("0xfeed"));

        QVERIFY(backend.setSessionSnapshotsEnabled(false));
        QCOMPARE(backend.property("sessionSnapshotsEnabled").toBool(), false);
        QVERIFY(QMetaObject::invokeMethod(&backend, "handleHyprlandEvent"));
        QTest::qWait(2000);
        QCOMPARE(backend.property("sessionSnapshots").toList().size(), 2);
    }
    void workspacePinsResolveGeneratedSlots()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.setWorkspacePlan("3", "WAM WD215I09", true));
        QCOMPARE(backend.workspacePlan().size(), 10);
        QCOMPARE(backend.workspacePlan().at(2).toMap().value("monitor").toString(), QString("WAM WD215I09"));
        QVERIFY(backend.setApplicationPlacement("demo", {{"name", "Demo"}, {"workspace", "3"}}));
        QVERIFY2(backend.setApplicationPlacement("plain", {{"name", "Plain"}, {"workspace", "5"}}),
                 qPrintable(backend.status()));
        QVERIFY(backend.workspacePlan().at(2).toMap().value("apps").toStringList().contains("demo"));
        // The catalogue merges rules with running/installed apps.
        bool found = false;
        for (const auto &entry : backend.applicationCatalog()) {
            const auto row = entry.toMap();
            if (row.value("class").toString() != "demo") continue;
            found = true;
            QVERIFY(row.value("pinned").toBool());
            QCOMPARE(row.value("workspace").toString(), QString("3"));
        }
        QVERIFY(found);

        QFile module(fixture.dir.filePath("windowtuner.lua")); QVERIFY(module.open(QIODevice::ReadOnly));
        const auto lua = QString::fromUtf8(module.readAll());
        module.close();
        QVERIFY2(lua.contains("__windowtuner_ws(\"3\", \"desc:WAM WD215I09\")"), lua.toUtf8().constData());
        QVERIFY(lua.contains("workspace = \"5\""));
        // A pinned slot must never silently become another workspace.
        QVERIFY(!backend.setApplicationPlacement("demo", {{"workspace", "11"}}));
        QVERIFY(backend.status().contains("1 to 10"));
        // Removing a placement-only rule leaves no stale module block.
        QVERIFY(backend.clearApplicationPlacement("demo"));
        QVERIFY(module.open(QIODevice::ReadOnly));
        const auto cleaned = QString::fromUtf8(module.readAll());
        module.close();
        QVERIFY(!cleaned.contains("__windowtuner_ws(\"3\""));
        QVERIFY(backend.workspacePlan().at(2).toMap().value("apps").toStringList().isEmpty());
        QVERIFY(backend.removeWorkspacePlan("3"));
        QCOMPARE(backend.workspacePlan().at(2).toMap().value("monitor").toString(), QString());
    }
    void monitorLayoutKeepsOrReverts()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\n"
            "if [ \"$2\" = clients ]; then echo '[]'; "
            "elif [ \"$2\" = monitors ]; then echo '[{\"id\":0,\"name\":\"DP-1\",\"description\":\"WAM WD215I09\","
            "\"width\":1920,\"height\":1080,\"x\":0,\"y\":0,\"scale\":1,\"transform\":0,\"refreshRate\":75.0,"
            "\"availableModes\":[\"1920x1080@75.00Hz\",\"1280x720@60.00Hz\"],\"reserved\":[0,0,0,0]}]'; "
            "elif [ \"$2\" = version ]; then echo '{\"tag\":\"fixture-lua\"}'; "
            "elif [ \"$1\" = configerrors ]; then echo '[]'; fi\nexit 0\n", true));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QCOMPARE(backend.monitors().size(), 1);
        const auto plan = backend.monitorPlan();
        QCOMPARE(plan.size(), 1);
        QCOMPARE(plan.first().toMap().value("description").toString(), QString("WAM WD215I09"));
        QVERIFY(!plan.first().toMap().value("saved").toBool());
        QVERIFY(plan.first().toMap().value("modes").toStringList().contains("1280x720@60.00"));

        QVERIFY2(backend.applyMonitorLayout("WAM WD215I09",
            {{"mode", "1280x720@60.00"}, {"x", 1920}, {"y", 0}, {"scale", 1.25}, {"transform", 1}, {"enabled", true}}),
            qPrintable(backend.status()));
        QVERIFY(backend.monitorRevertPending());
        auto saved = backend.monitorPlan().first().toMap();
        QVERIFY(saved.value("saved").toBool());
        QCOMPARE(saved.value("mode").toString(), QString("1280x720@60.00"));
        QCOMPARE(saved.value("x").toInt(), 1920);
        QCOMPARE(saved.value("scale").toDouble(), 1.25);
        QCOMPARE(saved.value("transform").toInt(), 1);
        QFile module(fixture.dir.filePath("windowtuner.lua")); QVERIFY(module.open(QIODevice::ReadOnly));
        const auto lua = QString::fromUtf8(module.readAll());
        module.close();
        QVERIFY2(lua.contains("install_windowtuner_appspace"), lua.toUtf8().constData());
        QVERIFY(lua.contains("WAM WD215I09"));
        QVERIFY(lua.contains("1280x720@60.00"));

        // A second change while waiting must still revert to the layout from
        // before the first change, not to the intermediate one.
        QVERIFY(backend.applyMonitorLayout("WAM WD215I09", {{"x", 640}}));
        QVERIFY(backend.monitorRevertPending());
        QVERIFY(backend.revertMonitorLayout());
        QVERIFY(!backend.monitorRevertPending());
        QVERIFY(!backend.monitorPlan().first().toMap().value("saved").toBool());
        QVERIFY(backend.revertMonitorLayout()); // idempotent when nothing is pending

        QVERIFY(backend.applyMonitorLayout("WAM WD215I09", {{"mode", "1920x1080@75.00"}, {"scale", 1.0}}));
        QVERIFY(backend.keepMonitorLayout());
        QVERIFY(!backend.monitorRevertPending());
        saved = backend.monitorPlan().first().toMap();
        QVERIFY(saved.value("saved").toBool());
        QCOMPARE(saved.value("scale").toDouble(), 1.0);
        // Keeping must leave the file alone: a later change reverts to this one.
        QVERIFY(backend.applyMonitorLayout("WAM WD215I09", {{"scale", 2.0}}));
        QVERIFY(backend.revertMonitorLayout());
        QCOMPARE(backend.monitorPlan().first().toMap().value("scale").toDouble(), 1.0);
    }
    void desktopMapPositionsMonitorsAndStagesSafeMoves()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\n"
            "if [ \"$2\" = clients ]; then echo '[]'; "
            "elif [ \"$2\" = monitors ]; then echo '["
            "{\"id\":0,\"name\":\"DP-1\",\"description\":\"Left display\",\"width\":1920,\"height\":1080,\"x\":-1920,\"y\":0,\"scale\":1,\"transform\":0,\"refreshRate\":60.0,\"activeWorkspace\":{\"id\":1,\"name\":\"1\"}},"
            "{\"id\":1,\"name\":\"HDMI-A-1\",\"description\":\"Right display\",\"width\":1280,\"height\":720,\"x\":0,\"y\":160,\"scale\":1,\"transform\":0,\"refreshRate\":60.0,\"activeWorkspace\":{\"id\":2,\"name\":\"2\"}}]'; "
            "elif [ \"$2\" = version ]; then echo '{\"tag\":\"fixture-lua\"}'; "
            "elif [ \"$1\" = configerrors ]; then echo '[]'; fi\nexit 0\n", true));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QCOMPARE(backend.monitors().size(), 2);
        QVERIFY(backend.setPreference("onboarded", true));
        QVERIFY(backend.setWorkspacePlan("1", "Left display", true));
        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->resize(1280, 900);
        QVERIFY(QMetaObject::invokeMethod(window, "navigate", Q_ARG(QVariant, -2)));
        QTest::qWait(150);
        window->grabWindow();

        auto *map = visualItem(window->contentItem(), "desktopMap"); QVERIFY(map);
        auto *left = visualItem(window->contentItem(), "desktopMapMonitor0"); QVERIFY(left);
        auto *right = visualItem(window->contentItem(), "desktopMapMonitor1"); QVERIFY(right);
        QVERIFY2(right->x() > left->x(), "Monitor cards should reflect their left-to-right desktop coordinates.");
        QVERIFY2(right->y() > left->y(), "Monitor cards should reflect their vertical desktop offset.");
        const qreal originalSeparation = right->x() - left->x();
        QCOMPARE(backend.monitorPlan().first().toMap().value("x").toInt(), -1920);
        QVERIFY(QMetaObject::invokeMethod(map, "setPreviewPosition",
            Q_ARG(QVariant, QString("Right display")),
            Q_ARG(QVariant, -1000), Q_ARG(QVariant, 0)));
        auto *overlapWarning = visualItem(window->contentItem(), "desktopMapOverlapWarning");
        QVERIFY(overlapWarning && overlapWarning->isVisible());
        auto *apply = visualItem(window->contentItem(), "desktopMapApplyAction"); QVERIFY(apply);
        QVERIFY(!apply->isEnabled());
        auto *cancel = visualItem(window->contentItem(), "desktopMapCancelAction"); QVERIFY(cancel);
        QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
        QTRY_VERIFY(!overlapWarning->isVisible());

        auto *moveLeft = visualItem(window->contentItem(), "desktopMapMoveLeft0"); QVERIFY(moveLeft);
        QVERIFY(moveLeft->isVisible());
        QVERIFY(QMetaObject::invokeMethod(moveLeft, "clicked"));
        QTRY_VERIFY(map->property("draftPositions").isValid());
        QTRY_VERIFY(right->x() - left->x() > originalSeparation + 10);
        QVERIFY(apply->isEnabled());
        QCOMPARE(backend.monitorPlan().first().toMap().value("x").toInt(), -1920);

        QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
        QTRY_COMPARE(right->x() - left->x(), originalSeparation);
        QCOMPARE(backend.monitorPlan().first().toMap().value("x").toInt(), -1920);
        QVERIFY(!backend.monitorRevertPending());

        QVERIFY(QMetaObject::invokeMethod(moveLeft, "clicked"));
        QVERIFY(apply->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
        QTRY_VERIFY(backend.monitorRevertPending());
        QCOMPARE(backend.monitorPlan().first().toMap().value("x").toInt(), -2020);
        auto *revert = visualItem(window->contentItem(), "desktopMapRevertAction"); QVERIFY(revert);
        QVERIFY(revert->isVisible());
        QVERIFY(QMetaObject::invokeMethod(revert, "clicked"));
        QTRY_VERIFY(!backend.monitorRevertPending());
        QCOMPARE(backend.monitorPlan().first().toMap().value("x").toInt(), -1920);

        auto *pinDialog = window->findChild<QObject *>("workspacePinDialog");
        QVERIFY(pinDialog);
        auto dragWorkspace = [&](const QString &slot, const QString &display) {
            auto *card = visualItem(window->contentItem(), "desktopMapWorkspace" + slot);
            auto *tile = visualItem(window->contentItem(), "desktopMapMonitor" + display);
            QVERIFY(card && tile);
            const auto from = card->mapToScene(QPointF(card->width() / 2, card->height() / 2)).toPoint();
            const auto to = tile->mapToScene(QPointF(tile->width() / 2, 30)).toPoint();
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
            QTest::mouseMove(window, (from + to) / 2);
            QTest::mouseMove(window, to);
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        };
        QTest::qWait(100);
        dragWorkspace("1", "1");
        QTRY_VERIFY(pinDialog->property("opened").toBool());
        auto *preview = visualItem(window->contentItem(), "workspacePinPreview");
        QVERIFY(preview);
        QVERIFY(preview->property("text").toString().contains("Left display → Right display"));
        const auto firstPin = [&] { return backend.workspacePlan().first().toMap(); };
        QCOMPARE(firstPin().value("monitor").toString(), QString("Left display"));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!pinDialog->property("visible").toBool());
        QCOMPARE(firstPin().value("monitor").toString(), QString("Left display"));
        QVERIFY(firstPin().value("persistent").toBool());

        dragWorkspace("1", "1");
        QTRY_VERIFY(pinDialog->property("opened").toBool());
        auto *savePin = visualItem(window->contentItem(), "saveWorkspacePinAction");
        QVERIFY(savePin && savePin->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(savePin, "clicked"));
        QTRY_VERIFY(!pinDialog->property("visible").toBool());
        QCOMPARE(firstPin().value("monitor").toString(), QString("Right display"));
        QVERIFY(firstPin().value("persistent").toBool());
        QVERIFY(!backend.monitorRevertPending());
        QCOMPARE(warnings.size(), 0);
    }
    void startupOnlyRulesMoveOnlyLoginWindows()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.setWorkspacePlan("2", "WAM WD215I09", false));
        QVERIFY2(backend.setApplicationPlacement("late",
            {{"name", "Late"}, {"workspace", "2"}, {"startupOnly", true}, {"startupWindow", 30}}),
            qPrintable(backend.status()));
        QFile module(fixture.dir.filePath("windowtuner.lua")); QVERIFY(module.open(QIODevice::ReadOnly));
        const auto lua = QString::fromUtf8(module.readAll());
        module.close();
        QVERIFY(lua.contains("hl.window_rule({"));
        // A pinned workspace would catch every later window, so the rule must
        // not carry one; the runtime handler does the placement instead.
        QVERIFY2(!lua.contains("workspace = __windowtuner_ws"), lua.toUtf8().constData());
        QVERIFY(lua.contains("install_windowtuner_appspace"));
        QVERIFY(lua.contains("within"));
        QVERIFY(lua.contains("late"));
        // Enabling autostart without a command or desktop entry is refused.
        QVERIFY(!backend.setApplicationPlacement("late", {{"workspace", ""}, {"autostart", true}}));
    }
    void initTestCase()
    {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
        QQuickStyle::setStyle("Material");
    }
    void onboardingSettingsSearchAndDirtyGuard()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto *welcome = window->findChild<QObject *>("welcomeDialog");
        QVERIFY(welcome);
        QTRY_VERIFY(welcome->property("opened").toBool());
        for (int i = 0; i < 4; ++i) {
            auto *next = visualItem(window->contentItem(), "welcomeNext"); QVERIFY(next);
            QVERIFY(QMetaObject::invokeMethod(next, "clicked"));
        }
        QTRY_VERIFY(!welcome->property("opened").toBool());
        QVERIFY(backend.preference("onboarded", false).toBool());
        QVERIFY(!window->property("advancedMode").toBool());
        auto *searchToggle = visualItem(window->contentItem(), "settingsSearchToggle"); QVERIFY(searchToggle);
        QVERIFY(QMetaObject::invokeMethod(searchToggle, "clicked"));
        QVERIFY(window->property("settingsSearch").toBool());
        auto *header = visualItem(window->contentItem(), "headerSearch"); QVERIFY(header);
        header->forceActiveFocus();
        for (char key : QByteArray("opacity")) QTest::keyClick(window, key);
        QTRY_COMPARE(window->property("settingResults").value<QJSValue>().toVariant().toList().size(), 1);
        const QVariant destination = QVariantMap{{"key", "opacity"}, {"page", 2}};
        QVERIFY(QMetaObject::invokeMethod(window, "jumpToSetting", Q_ARG(QVariant, destination)));
        QCOMPARE(window->property("category").toInt(), 2);
        QVariantMap preset{{"layout", "floating"}, {"width", 720}, {"height", 480}, {"borderEnabled", true}, {"borderSize", 0}};
        QVERIFY(QMetaObject::invokeMethod(window, "restorePreset", Q_ARG(QVariant, preset)));
        QTRY_VERIFY(window->property("dirty").toBool());
        QVariant returned;
        QVERIFY(QMetaObject::invokeMethod(window, "options", Q_RETURN_ARG(QVariant, returned)));
        QCOMPARE(returned.value<QJSValue>().toVariant().toMap().value("borderSize").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(window, "chooseWindow", Q_ARG(QVariant, 0)));
        auto *discard = window->findChild<QObject *>("discardDialog"); QVERIFY(discard);
        QTRY_VERIFY(discard->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(discard, "reject"));
        QVERIFY(window->property("dirty").toBool());
        window->setProperty("textScale", 1.2);
        QTest::qWait(100);
        window->grabWindow();
        QCOMPARE(window->minimumWidth(), 1032);
        auto *bar = visualItem(window->contentItem(), "ruleActionBar"); QVERIFY(bar);
        QVERIFY(bar->mapToScene(QPointF(bar->width(), bar->height())).y() <= window->height() + 1);
        QCOMPARE(warnings.size(), 0);
    }
    void toolsDialogsRenderAtSupportedSizes()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        const auto oldHome = qgetenv("XDG_DATA_HOME"), oldDirs = qgetenv("XDG_DATA_DIRS");
        const auto restoreEnv = qScopeGuard([&] {
            if (oldHome.isNull()) qunsetenv("XDG_DATA_HOME"); else qputenv("XDG_DATA_HOME", oldHome);
            if (oldDirs.isNull()) qunsetenv("XDG_DATA_DIRS"); else qputenv("XDG_DATA_DIRS", oldDirs);
        });
        qputenv("XDG_DATA_HOME", fixture.dir.filePath("data").toUtf8());
        qputenv("XDG_DATA_DIRS", fixture.dir.filePath("data").toUtf8());
        QVERIFY(fixture.write("data/applications/demo.desktop", "[Desktop Entry]\nType=Application\nName=Demo Editor\nStartupWMClass=org.example.Editor\n"));
        QVERIFY(fixture.write("data/applications/filename-guess.desktop", "[Desktop Entry]\nType=Application\nName=Filename Guess\n"));
        QVERIFY(fixture.write("clients.json",
            R"([{"address":"0x1","class":"foot-current","initialClass":"foot","title":"Work","initialTitle":"Work","workspace":{"name":"1"},"at":[10,20],"size":[800,600],"floating":true}])"));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.setPreference("onboarded", true));
        const QVariantMap settings{{"layout", "floating"}, {"width", 600}, {"height", 420}, {"matchTitle", false}};
        QVERIFY(backend.saveRule(settings));
        QVERIFY(backend.createBackup("Demo snapshot"));
        QVERIFY(backend.savePreset("Demo preset", settings));
        QVERIFY(backend.setWorkspacePlan("2", "Profile display", true));
        QVERIFY(backend.saveProfile("Demo profile"));
        const auto profileRules = backend.exportRules();
        QVERIFY(backend.setWorkspacePlan("2", "Current display", false));
        QVERIFY(backend.duplicateRule(0));
        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        const auto screenshots = qEnvironmentVariable("WINDOWTUNER_TEST_SCREENSHOTS");
        const QStringList names{"safetyDialog", "placementDesigner", "inspectorDialog", "classDiscoveryDialog", "workflowDialog", "profileSwitcherDialog", "workspacePinDialog", "managementDialog", "appSpaceDialog", "healthDialog"};
        for (const auto &size : QList<QSize>{{860, 680}, {1120, 820}, {1440, 900}}) {
            window->resize(size);
            QTest::qWait(100);
            if (!screenshots.isEmpty()) {
                QDir().mkpath(screenshots);
                QVERIFY(window->grabWindow().save(screenshots + "/windows-"
                    + QString("%1x%2.png").arg(size.width()).arg(size.height())));
            }
            for (const auto &name : names) {
                auto *popup = window->findChild<QObject *>(name); QVERIFY2(popup, qPrintable(name));
                QVERIFY(QMetaObject::invokeMethod(popup, "open"));
                QTRY_VERIFY(popup->property("opened").toBool());
                window->grabWindow();
                QVERIFY(popup->property("width").toDouble() <= window->width());
                QVERIFY(popup->property("height").toDouble() <= window->height());
                if (name == "appSpaceDialog")
                    QCOMPARE(qRound(popup->property("width").toDouble()), qMin(size.width() - 24, 1360));
                auto *scroll = popup->findChild<QQuickItem *>("toolScroll"); QVERIFY(scroll);
                QVERIFY(scroll->property("contentWidth").toDouble() <= scroll->property("availableWidth").toDouble() + 1);
                const auto rendered = window->grabWindow();
                const auto backdrop = rendered.pixelColor(qRound(popup->property("x").toDouble() + 4),
                                                           qRound(popup->property("y").toDouble() + 100));
                QVERIFY2(backdrop.red() < 70, "Dialog body must be opaque and dark.");
                if (name == "placementDesigner") {
                    popup->setProperty("placement", QVariantMap{{"enabled", true}, {"monitor", "DP-1"}, {"unit", "percent"}, {"width", 50}, {"height", 50}, {"anchor", "bottom-right"}, {"margin", 24}});
                    QTest::qWait(50);
                    const auto raw = popup->property("geometry");
                    const auto geometry = raw.metaType().id() == qMetaTypeId<QJSValue>() ? raw.value<QJSValue>().toVariant().toMap() : raw.toMap();
                    QCOMPARE(geometry.value("width").toInt(), 960);
                    if (size == QSize(1120, 820)) {
                        window->grabWindow();
                        auto *preview = visualItem(window->contentItem(), "placementWindow"); QVERIFY(preview);
                        const auto start = preview->mapToScene(QPointF(preview->width() / 2, preview->height() / 2)).toPoint();
                        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
                        QTest::mouseMove(window, start - QPoint(20, 20), 50);
                        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start - QPoint(20, 20));
                        auto p = popup->property("placement");
                        auto placement = p.metaType().id() == qMetaTypeId<QJSValue>() ? p.value<QJSValue>().toVariant().toMap() : p.toMap();
                        QCOMPARE(placement.value("anchor").toString(), QString("custom"));
                        QVERIFY(placement.value("x").toInt() < geometry.value("x").toInt());
                        window->grabWindow();
                        auto *handle = visualItem(window->contentItem(), "placementResizeHandle"); QVERIFY(handle);
                        const auto corner = handle->mapToScene(QPointF(8, 8)).toPoint();
                        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, corner);
                        QTest::mouseMove(window, corner - QPoint(20, 10), 50);
                        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, corner - QPoint(20, 10));
                        p = popup->property("placement");
                        placement = p.metaType().id() == qMetaTypeId<QJSValue>() ? p.value<QJSValue>().toVariant().toMap() : p.toMap();
                        QCOMPARE(placement.value("unit").toString(), QString("pixels"));
                        QVERIFY(placement.value("width").toInt() < 960);
                    }
                    popup->setProperty("placement", QVariantMap{{"enabled", true}, {"monitor", "DP-1"},
                        {"unit", "pixels"}, {"width", 600}, {"height", 420}, {"anchor", "custom"},
                        {"x", 240}, {"y", 180}});
                    auto *positionX = visualItem(window->contentItem(), "placementX");
                    auto *positionY = visualItem(window->contentItem(), "placementY");
                    QVERIFY(positionX && positionY);
                    QVERIFY(positionX->isVisible() && positionY->isVisible());
                    QCOMPARE(positionX->property("value").toInt(), 240);
                    QCOMPARE(positionY->property("value").toInt(), 180);
                    positionX->setProperty("value", 280);
                    QVERIFY(QMetaObject::invokeMethod(positionX, "valueModified", Qt::DirectConnection));
                    const auto updatedRaw = popup->property("placement");
                    const auto updatedPlacement = updatedRaw.metaType().id() == qMetaTypeId<QJSValue>()
                        ? updatedRaw.value<QJSValue>().toVariant().toMap() : updatedRaw.toMap();
                    QCOMPARE(updatedPlacement.value("x").toInt(), 280);
                }
                if (name == "appSpaceDialog") {
                    // Every tab must lay out without warnings or clipped content.
                    auto *tabs = visualItem(window->contentItem(), "appSpaceTabs");
                    QVERIFY(tabs);
                    const QStringList tabItems{"appSpaceSearch", "appSpaceWorkspaceList",
                                               "appSpaceMonitorList", "appSpaceImportPreview"};
                    for (int tab = 0; tab < tabItems.size(); ++tab) {
                        tabs->setProperty("currentIndex", tab);
                        QTest::qWait(80);
                        auto *page = visualItem(window->contentItem(), tabItems[tab]);
                        QVERIFY2(page, qPrintable(tabItems[tab]));
                        QVERIFY(page->isVisible());
                        if (tab == 1) {
                            auto *summary = visualItem(window->contentItem(), "workspaceAppsSummary");
                            QVERIFY(summary && summary->isVisible());
                            QVERIFY(summary->property("wrapMode").toInt() != 0);
                        }
                        window->grabWindow();
                        if (!screenshots.isEmpty())
                            QVERIFY(window->grabWindow().save(screenshots + "/appSpaceDialog-"
                                + QString::number(tab) + QString("-%1x%2.png").arg(size.width()).arg(size.height())));
                    }
                    tabs->setProperty("currentIndex", 0);
                }
                if (!screenshots.isEmpty()) {
                    QDir().mkpath(screenshots);
                    QVERIFY(window->grabWindow().save(screenshots + "/" + name + QString("-%1x%2.png").arg(size.width()).arg(size.height())));
                }
                if (name == "healthDialog") {
                    auto *toggle = visualItem(window->contentItem(), "toggleConfigErrorsAction");
                    auto *details = visualItem(window->contentItem(), "configErrorsDetails");
                    QVERIFY(toggle && details);
                    QVERIFY(!details->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(toggle, "click"));
                    QTRY_VERIFY(details->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(toggle, "click"));
                    QTRY_VERIFY(!details->isVisible());
                    popup->setProperty("verification", QVariantMap{{"details", "Fixture verification details"}});
                    auto *verificationToggle = visualItem(window->contentItem(), "toggleVerificationDetailsAction");
                    auto *verificationDetails = visualItem(window->contentItem(), "verificationDetails");
                    QVERIFY(verificationToggle && verificationDetails);
                    QVERIFY(verificationToggle->isVisible());
                    QVERIFY(!verificationDetails->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(verificationToggle, "click"));
                    QTRY_VERIFY(verificationDetails->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(verificationToggle, "click"));
                    QTRY_VERIFY(!verificationDetails->isVisible());
                }
                if (name == "inspectorDialog") {
                    auto *observed = visualItem(window->contentItem(), "inspectorObservedWindow");
                    QVERIFY(observed && observed->property("text").toString().contains("800 × 600"));
                    auto *conflictMap = visualItem(window->contentItem(), "ruleConflictMap");
                    QVERIFY(conflictMap && conflictMap->isVisible());
                    auto *evaluationToggle = visualItem(window->contentItem(), "toggleEvaluationDetailsAction");
                    auto *evaluationDetails = visualItem(window->contentItem(), "evaluationDetails");
                    QVERIFY(evaluationToggle && evaluationDetails);
                    QVERIFY(!evaluationDetails->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(evaluationToggle, "click"));
                    QTRY_VERIFY(evaluationDetails->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(evaluationToggle, "click"));
                    QTRY_VERIFY(!evaluationDetails->isVisible());
                    auto *toggle = visualItem(window->contentItem(), "toggleWindowPropertiesAction");
                    auto *details = visualItem(window->contentItem(), "windowPropertiesDetails");
                    QVERIFY(toggle && details);
                    QVERIFY(!details->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(toggle, "click"));
                    QTRY_VERIFY(details->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(toggle, "click"));
                    QTRY_VERIFY(!details->isVisible());
                }
                if (name == "profileSwitcherDialog") {
                    QVERIFY(QMetaObject::invokeMethod(window, "markClean"));
                    auto *search = visualItem(window->contentItem(), "profileQuickSearch");
                    auto *list = visualItem(window->contentItem(), "profileQuickList");
                    auto *switchAction = visualItem(window->contentItem(), "profileQuickSwitchAction");
                    QVERIFY(search && list && switchAction);
                    QVERIFY(search->isVisible());
                    QCOMPARE(backend.profiles().size(), 1);
                    QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
                    QVERIFY(QMetaObject::invokeMethod(popup, "choose",
                        Q_ARG(QVariant, backend.profiles().first())));
                    QCOMPARE(popup->property("selectedId").toString(),
                        backend.profiles().first().toMap().value("id").toString());
                    QVERIFY(switchAction->isEnabled());
                    const auto pin = [&] { return backend.workspacePlan().at(1).toMap(); };
                    const auto activeRules = backend.exportRules();
                    auto *diffText = visualItem(window->contentItem(), "profileQuickDiffText");
                    QVERIFY(diffText);
                    const auto text = diffText->property("text").toString();
                    QVERIFY(text.contains("Rule #2"));
                    QVERIFY(text.contains("Workspace 2 / monitor: \"Current display\" → \"Profile display\""));
                    QVERIFY(text.contains("Workspace 2 / persistent: false → true"));
                    QVERIFY(!backend.applyProfileReview(popup->property("selectedId").toString(), "stale-preview"));
                    QCOMPARE(backend.exportRules(), activeRules);
                    QVERIFY(QMetaObject::invokeMethod(switchAction, "clicked"));
                    auto *confirmation = window->findChild<QObject *>("profileQuickSwitchConfirmation");
                    QVERIFY(confirmation);
                    QTRY_VERIFY(confirmation->property("opened").toBool());
                    QVERIFY(QMetaObject::invokeMethod(confirmation, "reject"));
                    QTRY_VERIFY(!confirmation->property("visible").toBool());
                    QCOMPARE(pin().value("monitor").toString(), QString("Current display"));
                    QCOMPARE(backend.exportRules(), activeRules);
                    if (size == QSize(1440, 900)) {
                        const auto unchangedDisplays = backend.monitorPlan();
                        QVERIFY(QMetaObject::invokeMethod(switchAction, "clicked"));
                        QTRY_VERIFY(confirmation->property("opened").toBool());
                        QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
                        QTRY_VERIFY(!popup->property("opened").toBool());
                        QCOMPARE(pin().value("monitor").toString(), QString("Profile display"));
                        QVERIFY(pin().value("persistent").toBool());
                        QCOMPARE(backend.exportRules(), profileRules);
                        QCOMPARE(backend.monitorPlan(), unchangedDisplays);
                        QVERIFY(backend.undo());
                        QCOMPARE(pin().value("monitor").toString(), QString("Current display"));
                        QCOMPARE(backend.exportRules(), activeRules);
                    }
                }
                if (name == "classDiscoveryDialog") {
                    const auto savedRules = backend.exportRules();
                    auto *observed = visualItem(window->contentItem(), "discoveryObservedClass");
                    auto *picker = visualItem(window->contentItem(), "discoveryHintPicker");
                    auto *hint = visualItem(window->contentItem(), "discoveryHintClass");
                    auto *field = visualItem(window->contentItem(), "discoveryClassField");
                    auto *draft = visualItem(window->contentItem(), "discoveryDraftClass");
                    auto *test = visualItem(window->contentItem(), "testDiscoveredClass");
                    auto *result = visualItem(window->contentItem(), "discoveryResult");
                    QVERIFY(observed && picker && hint && field && draft && test && result);
                    QVERIFY(observed->property("text").toString().contains("Current class: foot-current · Initial class: foot"));
                    QCOMPARE(draft->property("text").toString(), QString("foot-current"));
                    QVERIFY(picker->setProperty("currentIndex", 0));
                    QVERIFY(QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, 0)));
                    QVERIFY(hint->property("text").toString().contains("StartupWMClass (unverified): org.example.Editor"));
                    QVERIFY(hint->property("text").toString().contains("Differs from the observed"));
                    auto *search = visualItem(window->contentItem(), "discoveryHintSearch");
                    QVERIFY(search && search->setProperty("text", "Filename Guess"));
                    QTRY_COMPARE(picker->property("count").toInt(), 1);
                    QVERIFY(picker->setProperty("currentIndex", 0));
                    QVERIFY(QMetaObject::invokeMethod(picker, "activated", Q_ARG(int, 0)));
                    QVERIFY(hint->property("text").toString().contains("Filename guess (unverified): filename-guess"));
                    QVERIFY(QMetaObject::invokeMethod(test, "clicked"));
                    QVERIFY(result->property("text").toString().contains("1 of 1"));
                    QVERIFY(draft->setProperty("text", "org.example.Editor"));
                    QVERIFY(QMetaObject::invokeMethod(test, "clicked"));
                    QVERIFY(result->property("text").toString().contains("0 of 1"));
                    QVERIFY(field->setProperty("currentIndex", 1));
                    QVERIFY(QMetaObject::invokeMethod(field, "activated", Q_ARG(int, 1)));
                    QCOMPARE(draft->property("text").toString(), QString("foot"));
                    QVERIFY(QMetaObject::invokeMethod(test, "clicked"));
                    QVERIFY(result->property("text").toString().contains("1 of 1"));
                    QCOMPARE(backend.exportRules(), savedRules);
                }
                if (name == "workspacePinDialog") {
                    auto *monitorChoice = visualItem(window->contentItem(), "workspacePinMonitorChoice");
                    auto *savePin = visualItem(window->contentItem(), "saveWorkspacePinAction");
                    QVERIFY(monitorChoice && savePin);
                    QVERIFY(QMetaObject::invokeMethod(window, "openWorkspacePin",
                        Q_ARG(QVariant, QString("2")), Q_ARG(QVariant, QString()),
                        Q_ARG(QVariant, false), Q_ARG(QVariant, QString())));
                    QVERIFY(popup->property("opened").toBool());
                    QVERIFY(!savePin->isEnabled());
                }
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                QTRY_VERIFY(!popup->property("opened").toBool());
            }
            auto *launcher = window->findChild<QObject *>("toolsMenu"); QVERIFY(launcher);
            QVERIFY(QMetaObject::invokeMethod(launcher, "open"));
            QTRY_VERIFY(launcher->property("opened").toBool());
            QVERIFY(launcher->property("width").toDouble() <= window->width());
            QVERIFY(launcher->property("height").toDouble() <= window->height());
            auto *appSpaceTile = visualItem(window->contentItem(), "appSpaceMenuEntry");
            QVERIFY(appSpaceTile && appSpaceTile->isVisible());
            if (!screenshots.isEmpty() && size == QSize(1120, 820))
                QVERIFY(window->grabWindow().save(screenshots + "/toolsLauncher-1120x820.png"));
            QVERIFY(QMetaObject::invokeMethod(launcher, "close"));
            QTRY_VERIFY(!launcher->property("opened").toBool());
        }
        QCOMPARE(backend.installedApplications().size(), 2);
        QCOMPARE(backend.installedApplications().first().toMap().value("class").toString(), QString("org.example.Editor"));
        QCOMPARE(warnings.size(), 0);
    }
    void historySurvivesRestart()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        {
            Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
            QVERIFY(backend.saveRule({{"layout", "tiled"}, {"matchTitle", false}}));
            QVERIFY(backend.createBackup("Before deletion"));
            QVERIFY(backend.deleteRule(0));
            QVERIFY(backend.canUndo());
        }
        Backend reopened(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QCOMPARE(reopened.savedRules().size(), 0);
        QCOMPARE(reopened.backups().size(), 1);
        QVERIFY(reopened.undo());
        QCOMPARE(reopened.savedRules().size(), 1);
        QVERIFY(reopened.redo());
        QCOMPARE(reopened.savedRules().size(), 0);
    }
    void reviewExactlyPredictsWrittenLua()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVariantMap settings{{"layout", "floating"}, {"matchTitle", false}, {"width", 640}, {"height", 480},
            {"match", QVariantMap{{"class", "foot"}}}};
        auto review = backend.reviewChanges(settings);
        QVERIFY(!QFile::exists(fixture.dir.filePath("windowtuner.json")));
        QVERIFY(backend.saveRule(settings));
        QFile module(fixture.dir.filePath("windowtuner.lua")); QVERIFY(module.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(module.readAll()), review.value("moduleAfter").toString());
        QFile config(fixture.dir.filePath("hyprland.lua")); QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(config.readAll()), review.value("configAfter").toString());
        module.close();
        settings["width"] = 800;
        review = backend.reviewChanges(settings, 0);
        QVERIFY(backend.updateRule(0, settings));
        QVERIFY(module.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(module.readAll()), review.value("moduleAfter").toString());
        module.close();
        settings["width"] = 900;
        review = backend.reviewChanges(settings);
        QVERIFY(backend.saveRule(settings));
        QVERIFY(module.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(module.readAll()), review.value("moduleAfter").toString());
        QCOMPARE(backend.savedRules().size(), 1);
    }
    void failedValidationPreservesFilesAndHistory()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.saveRule({{"layout", "tiled"}, {"matchTitle", false}}));
        const auto before = backend.exportRules();
        const auto historySize = backend.history().size();
        // An error that was already there is the baseline and must not block an
        // unrelated change; an error introduced *by* the change still rolls it
        // back. The fake compositor reports a problem only for disabled rules.
        const QString data = fixture.dir.filePath("windowtuner.json");
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\nif [ \"$1\" = configerrors ]; then "
            "if [ -f '" + data.toUtf8() + "' ] && grep -q '\"enabled\": false' '" + data.toUtf8() + "'; "
            "then echo 'fixture failure'; else echo '[]'; fi; fi\nexit 0\n", true));
        QVERIFY(!backend.setRuleEnabled(0, false));
        QCOMPARE(backend.exportRules(), before);
        QCOMPARE(backend.history().size(), historySize);
        QVERIFY(backend.savedRules().first().toMap().value("enabled").toBool());
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\nif [ \"$1\" = configerrors ]; then exit 1; fi\nexit 0\n", true));
        QVERIFY(!backend.deleteRule(0));
        QCOMPARE(backend.exportRules(), before);
    }
    void corruptRecoveryDataIsNeverApplied()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        const auto malformed = QJsonDocument(QJsonObject{{"past", QJsonArray{QJsonObject{
            {"entryPoint", fixture.dir.filePath("hyprland.lua")}}}}}).toJson();
        QVERIFY(fixture.write("windowtuner-history.json", malformed));
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(!backend.undo());
        QFile config(fixture.dir.filePath("hyprland.lua")); QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(config.readAll(), QByteArray("-- fixture config\n"));
        QVERIFY(fixture.write("windowtuner-history.json", "not-json"));
        QVERIFY(!backend.saveRule({{"layout", "tiled"}, {"matchTitle", false}}));
        QVERIFY(!QFile::exists(fixture.dir.filePath("windowtuner.json")));
    }
    void failedTrialRetainsRecoveryRecord()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\nif [ \"$2\" = clients ]; then cat '" +
            fixture.dir.filePath("clients.json").toUtf8() + "'; elif [ \"$1\" = eval ]; then echo 'error: denied'; fi\nexit 0\n", true));
        QVERIFY(!backend.tryGeometry({{"layout", "floating"}, {"width", 500}, {"height", 400}}));
        QVERIFY(backend.trialActive());
        QVERIFY(fixture.write("hyprctl", "#!/bin/sh\nexit 0\n", true));
        QVERIFY(backend.revertTrial());
        QVERIFY(!backend.trialActive());
        QVERIFY(!QFile::exists(fixture.dir.filePath("windowtuner.json")));
    }
    void invalidSettingsAreRejected_data()
    {
        QTest::addColumn<QVariantMap>("settings");
        QTest::newRow("negative width") << QVariantMap{{"width", -1}};
        QTest::newRow("nonnumeric height") << QVariantMap{{"height", "bad"}};
        QTest::newRow("invalid layout") << QVariantMap{{"layout", "magic"}};
        QTest::newRow("invalid opacity") << QVariantMap{{"opacityActive", 2}};
        QTest::newRow("invalid regex") << QVariantMap{{"classMatchMode", "regex"}, {"match", QVariantMap{{"class", "["}}}};
        QTest::newRow("invalid exclusion") << QVariantMap{{"excludeTitle", "["}};
        QTest::newRow("inverted limits") << QVariantMap{{"minSizeEnabled", true}, {"maxSizeEnabled", true}, {"minWidth", 1500}, {"maxWidth", 1000}};
        QTest::newRow("negative border") << QVariantMap{{"borderEnabled", true}, {"borderSize", -1}};
        QTest::newRow("unsupported setting") << QVariantMap{{"unknownEffect", true}};
        QTest::newRow("unsupported predicate") << QVariantMap{{"match", QVariantMap{{"class", "foot"}, {"pid", 42}}}};
        QTest::newRow("unsupported match mode") << QVariantMap{{"classMatchMode", "magic"}};
    }
    void invalidSettingsAreRejected()
    {
        QFETCH(QVariantMap, settings);
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(!backend.validateSettings(settings).isEmpty());
        QVERIFY(!backend.saveRule(settings));
        QVERIFY(!QFile::exists(fixture.dir.filePath("windowtuner.json")));
    }
    void auditsInstalledFeatureCapabilities()
    {
        if (QStandardPaths::findExecutable("Hyprland").isEmpty()) QSKIP("Hyprland is not installed.");
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        const auto capabilities = backend.auditCapabilities();
        QCOMPARE(capabilities.size(), 10);
        for (const auto &entry : capabilities) {
            const auto row = entry.toMap();
            QVERIFY2(row.value("status").toString() == "verified-config-only",
                     qPrintable(row.value("name").toString() + ": " + row.value("details").toString()));
        }
    }
    void verifiesDraftWithInstalledHyprland()
    {
        if (QStandardPaths::findExecutable("Hyprland").isEmpty()) QSKIP("Hyprland is not installed.");
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        const QVariantMap settings{{"layout", "floating"}, {"matchTitle", false}, {"width", 640}, {"height", 480},
            {"setPosition", true}, {"center", false}, {"x", 24}, {"y", 40}, {"monitor", "DP-1"},
            {"pin", true}, {"noBlur", true}, {"noShadow", true}, {"noInitialFocus", true}, {"noAnim", true},
            {"noDim", true}, {"noVrr", true}, {"persistentSize", true}, {"roundingEnabled", true}, {"rounding", 8},
            {"minSizeEnabled", true}, {"minWidth", 320}, {"minHeight", 200},
            {"maxSizeEnabled", true}, {"maxWidth", 1200}, {"maxHeight", 900},
            {"opacityEnabled", true}, {"opacityActive", 0.9}, {"opacityInactive", 0.8},
            {"borderEnabled", true}, {"borderSize", 2}, {"workspace", "3 silent"}};
        const auto result = backend.verifyDraft(settings);
        QVERIFY2(result.value("ok").toBool(), qPrintable(result.value("details").toString()));
        QFile config(fixture.dir.filePath("hyprland.lua")); QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(config.readAll(), QByteArray("-- fixture config\n"));
        QVERIFY(!QFile::exists(fixture.dir.filePath("windowtuner.json")));
    }
    void healthAndDiagnosticsDoNotLeakPrivateData()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QCOMPARE(backend.healthReport().value("version").toString(), QString("fixture-lua"));
        QVERIFY(!backend.healthReport().value("apiVerified").toBool());
        const QString diagnostic = backend.diagnosticReport();
        QVERIFY(!diagnostic.contains(fixture.dir.path()));
        QVERIFY(!diagnostic.contains("Work"));
        QVERIFY(!diagnostic.contains("foot"));
        QVERIFY(fixture.write("alternate.lua", "-- alternate\n"));
        QVERIFY(backend.configureEntryPoint(fixture.dir.filePath("alternate.lua")));
        QCOMPARE(backend.configEntryPoint(), fixture.dir.filePath("alternate.lua"));
        QVERIFY(!backend.configureEntryPoint("relative.lua"));
    }
    void managesOfflineRulesMetadataAndSelectiveImports()
    {
        DesktopFixture fixture; QVERIFY(fixture.initialize());
        Backend backend(fixture.dir.path(), fixture.dir.filePath("hyprland.lua"), fixture.dir.filePath("hyprctl"));
        QVERIFY(backend.selectOfflineTarget("org.example.Editor"));
        const QVariantMap settings{{"layout", "floating"}, {"matchTitle", false}, {"width", 700}, {"height", 500}};
        QVERIFY(backend.saveRule(settings));
        QCOMPARE(backend.savedRules().size(), 1);
        const auto id = backend.savedRules().first().toMap().value("id").toString();
        QVERIFY(backend.setRuleMetadata(id, "My editor", "Work", "code, work, code"));
        QCOMPARE(backend.savedRules().first().toMap().value("tags").toStringList().size(), 2);
        QVERIFY(backend.batchSetEnabled({id}, false));
        QVERIFY(!backend.savedRules().first().toMap().value("enabled").toBool());
        QVERIFY(!backend.batchSetEnabled({"nonexistent"}, true));
        QVERIFY(!backend.savedRules().first().toMap().value("enabled").toBool());
        const auto json = backend.exportSelected({id});
        QCOMPARE(backend.previewImport(json).value("count").toInt(), 1);
        QVERIFY(backend.importSelection(json, {0}, "merge"));
        QCOMPARE(backend.savedRules().size(), 2);
        QVERIFY(backend.savedRules()[0].toMap().value("id") != backend.savedRules()[1].toMap().value("id"));
        QVERIFY(backend.importSelection(json, {0}, "replace"));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(!backend.importSelection(json, {9}, "replace"));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(!backend.importSelection(json, {0}, "bogus"));
        QVERIFY(!backend.previewImport("[{}]").value("error").toString().isEmpty());
        auto editable = backend.loadRuleForEditing(0);
        editable["match"] = QVariantMap{{"class", "org.example.Other"}};
        QVERIFY(backend.updateRule(0, editable));
        QCOMPARE(backend.savedRules().first().toMap().value("match").toMap().value("class").toString(), QString("org.example.Other"));
    }
    void evaluatesAllMatchConditions()
    {
        const QVariantMap window{{"class", "foot"}, {"currentClass", "foot"},
            {"title", "Initial"}, {"currentTitle", "Work 12"}, {"workspace", "2"}, {"xwayland", false}};
        QVariantMap settings{{"match", QVariantMap{{"class", "foo"}, {"title", "^Work [0-9]+$"}, {"workspace", "2"}, {"xwayland", false}}},
            {"classMatchMode", "contains"}, {"titleMatchMode", "regex"}};
        QVERIFY(Backend::matchWindow(window, settings).value("matched").toBool());
        auto match = settings.value("match").toMap(); match["workspace"] = "3"; settings["match"] = match;
        QVERIFY(!Backend::matchWindow(window, settings).value("matched").toBool());
        settings["match"] = QVariantMap{{"class", "foot"}, {"unknown_field", true}};
        QVERIFY(Backend::matchWindow(window, settings).value("unknown").toBool());
        settings["match"] = QVariantMap{{"class", "foot"}}; settings["excludeTitle"] = "^Work.*";
        QVERIFY(!Backend::matchWindow(window, settings).value("matched").toBool());
        settings["excludeTitle"] = "["; QVERIFY(Backend::matchWindow(window, settings).value("unknown").toBool());
    }
    void resolvesScaledPlacementAndFallback()
    {
        const QVariantList monitors{QVariantMap{{"name", "DP-1"}, {"width", 3840}, {"height", 2160},
            {"scale", 2.0}, {"reserved", QVariantList{0, 40, 0, 0}}}};
        const QVariantMap p{{"enabled", true}, {"monitor", "missing"}, {"fallback", "DP-1"},
            {"unit", "percent"}, {"width", 50}, {"height", 50}, {"anchor", "bottom-right"}, {"margin", 24}};
        const auto geometry = Backend::placementGeometry(p, monitors);
        QCOMPARE(geometry.value("width").toInt(), 960);
        QCOMPARE(geometry.value("height").toInt(), 520);
        QCOMPARE(geometry.value("x").toInt(), 936);
        QCOMPARE(geometry.value("y").toInt(), 536);
        QCOMPARE(geometry.value("monitor").toString(), QString("DP-1"));
        QVERIFY(geometry.value("usedFallback").toBool());
        QVERIFY(Backend::placementGeometry(p, {}).contains("error"));
        auto locked = p; locked.insert("aspectLock", true); locked.insert("aspect", 2.0);
        QCOMPARE(Backend::placementGeometry(locked, monitors).value("height").toInt(), 480);
        auto invalid = p; invalid.insert("width", -4);
        QVERIFY(Backend::placementGeometry(invalid, monitors).contains("error"));
    }
    void controlStyleNavigationAndResponsiveLayout()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile config(dir.filePath("hyprland.lua"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("-- isolated UI test\n");
        config.close();
        QFile script(dir.filePath("hyprctl"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nif [ \"$1\" = eval ]; then printf '%s\\n' \"$2\" >> '"
            + dir.filePath("rehearsal.log").toUtf8() + "'; echo ok; exit 0; fi\n");
        script.write("#!/bin/sh\nif [ \"$2\" = clients ]; then echo '[{\"address\":\"0x1\",\"class\":\"foot\",\"initialClass\":\"foot\",\"title\":\"Terminal\",\"initialTitle\":\"Terminal\",\"workspace\":{\"name\":\"1\"},\"monitor\":0,\"at\":[135,240],\"size\":[800,600],\"floating\":true,\"pinned\":true},{\"address\":\"0x2\",\"class\":\"firefox\",\"initialClass\":\"firefox\",\"title\":\"Browser\",\"initialTitle\":\"Browser\",\"workspace\":{\"name\":\"1\"},\"monitor\":0,\"at\":[400,200],\"size\":[1000,700],\"floating\":false}]'; fi\nif [ \"$2\" = monitors ]; then echo '[{\"id\":0,\"name\":\"DP-1\",\"description\":\"Test Display\",\"x\":0,\"y\":0,\"width\":1920,\"height\":1080,\"scale\":1,\"activeWorkspace\":{\"id\":1,\"name\":\"1\"}}]'; fi\nexit 0\n");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QFile savedRules(dir.filePath("windowtuner.json"));
        QVERIFY(savedRules.open(QIODevice::WriteOnly));
        savedRules.write(R"([{
            "id":"ui-rule","name":"Terminal rule","class":"foot","title":"Terminal",
            "match":{"class":"foot","title":"Terminal"},
            "settings":{"layout":"floating","width":560,"height":400,"workspace":"2","pin":true,
                "opacityEnabled":true,"opacityActive":0.6,"opacityInactive":0.75,
                "borderEnabled":true,"borderSize":5,"roundingEnabled":true,"rounding":12,
                "noBlur":true,"noShadow":true,"noInitialFocus":true,"noAnim":true,
                "noDim":true,"noVrr":true,"persistentSize":true}
        }])");
        savedRules.close();
        Backend backend(dir.path(), config.fileName(), script.fileName());
        QVERIFY(backend.setPreference("onboarded", true));
        Theme theme;
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(window->property("windowsPage").toBool());
        std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual;
        findVisual = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) {
                if (auto *found = findVisual(child, name)) return found;
            }
            return nullptr;
        };
        auto item = [&](const char *name) { return findVisual(window->contentItem(), QString::fromLatin1(name)); };
        auto clickItem = [&](QQuickItem *target) {
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                              target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint());
        };
        auto navigate = [&](int section) {
            QVERIFY(QMetaObject::invokeMethod(window, "navigate", Q_ARG(QVariant, section)));
            // Layout polishing and Material animations run on subsequent frames.
            QTest::qWait(200);
            window->grabWindow();
        };
        QVERIFY(item("headerSearch"));
        QVERIFY(item("windowsPage")->isVisible());
        auto *desktopsNavigation = item("desktopsNavigation");
        QVERIFY(desktopsNavigation && desktopsNavigation->isVisible());
        clickItem(desktopsNavigation);
        QTRY_VERIFY(window->property("desktopsPage").toBool());
        auto *desktopsPage = item("desktopsPage");
        QVERIFY(desktopsPage && desktopsPage->isVisible());
        QCOMPARE(item("sectionNavigation4")->property("selected").toBool(), false);
        QCOMPARE(item("headerSearch")->property("text").toString(), QString());
        auto *workspaceTile = item("desktopMapWorkspace1");
        QVERIFY(workspaceTile && workspaceTile->isVisible());
        auto *activeWorkspaceLabel = item("desktopMapActiveWorkspace0");
        QVERIFY(activeWorkspaceLabel && activeWorkspaceLabel->isVisible());
        auto *openApps = item("desktopMapOpenWindows0");
        QVERIFY(openApps);
        QCOMPARE(openApps->property("text").toString(), QString("Open: Terminal, Browser"));
        QCOMPARE(backend.windows().size(), 2);
        clickItem(item("desktopScenesAction"));
        auto *scenesDialog = window->findChild<QObject *>("scenesDialog");
        QVERIFY(scenesDialog);
        QTRY_VERIFY(scenesDialog->property("opened").toBool());
        auto *sceneNameField = item("sceneNameField");
        auto *captureScene = item("captureSceneAction");
        QVERIFY(sceneNameField && captureScene);
        sceneNameField->setProperty("text", "UI scene");
        clickItem(captureScene);
        QTRY_COMPARE(backend.property("scenes").toList().size(), 1);
        auto *sceneRow = item("savedSceneRow");
        QVERIFY(sceneRow && sceneRow->isVisible());
        clickItem(sceneRow);
        auto *scenePreview = item("scenePreviewSummary");
        QVERIFY(scenePreview);
        QTRY_VERIFY(scenePreview->property("text").toString().contains("2 open window(s) match"));
        const QString scenesScreenshots = qEnvironmentVariable("WINDOWTUNER_TEST_SCREENSHOTS");
        if (!scenesScreenshots.isEmpty()) {
            QDir().mkpath(scenesScreenshots);
            QVERIFY(window->grabWindow().save(scenesScreenshots + "/scenes-1120x820.png"));
        }
        auto *applyScene = item("openSceneAction");
        QVERIFY(applyScene && applyScene->isEnabled());
        clickItem(applyScene);
        auto *applyConfirmation = window->findChild<QObject *>("applySceneConfirmation");
        QVERIFY(applyConfirmation);
        QTRY_VERIFY(applyConfirmation->property("opened").toBool());
        QVERIFY(item("applySceneConfirmationText")->property("text").toString()
                    .contains("No apps will be launched"));
        QTRY_VERIFY(item("restoreWindowChoice0") && item("restoreWindowChoice1"));
        QVERIFY(!item("applyConfirmedWindows")->isEnabled());
        clickItem(item("restoreWindowChoice0"));
        QVERIFY(item("applyConfirmedWindows")->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(applyConfirmation, "reject"));
        QTRY_VERIFY(!applyConfirmation->property("visible").toBool());
        const auto rehearsalLog = dir.filePath("rehearsal.log");
        QVERIFY(!QFileInfo::exists(rehearsalLog));
        QVERIFY(QMetaObject::invokeMethod(applyScene, "clicked"));
        QTRY_VERIFY(applyConfirmation->property("opened").toBool());
        QVERIFY(!item("restoreWindowChoice0")->property("checked").toBool());
        clickItem(item("restoreWindowChoice0"));
        clickItem(item("applyConfirmedWindows"));
        QTRY_VERIFY(!applyConfirmation->property("visible").toBool());
        QFile rehearsalDispatch(rehearsalLog);
        QVERIFY(rehearsalDispatch.open(QIODevice::ReadOnly));
        const auto dispatched = rehearsalDispatch.readAll();
        QVERIFY(dispatched.contains("address:0x1"));
        QVERIFY(!dispatched.contains("address:0x2"));
        QTest::qWait(250);
        auto *sessionToggle = item("sessionSnapshotsEnabled");
        QVERIFY(sessionToggle);
        QCOMPARE(sessionToggle->property("checked").toBool(), false);
        QVERIFY(QMetaObject::invokeMethod(scenesDialog, "close"));
        QTRY_VERIFY(!scenesDialog->property("opened").toBool());
        QTest::qWait(250);
        auto *manageApps = item("desktopManageAppsAction");
        QVERIFY(manageApps && manageApps->isVisible());
        clickItem(manageApps);
        auto *appSpace = window->findChild<QObject *>("appSpaceDialog");
        QVERIFY(appSpace);
        QTRY_VERIFY(appSpace->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(appSpace, "close"));
        QTRY_VERIFY(!appSpace->property("opened").toBool());
        QTest::qWait(250);
        clickItem(item("windowsNavigation"));
        QTRY_VERIFY(window->property("windowsPage").toBool());
        auto *searchScope = item("settingsSearchToggle");
        QVERIFY(searchScope);
        QCOMPARE(searchScope->property("text").toString(), QString("Switch to settings"));
        clickItem(searchScope);
        QTRY_VERIFY(window->property("settingsSearch").toBool());
        QCOMPARE(searchScope->property("text").toString(), QString("Switch to windows"));
        clickItem(searchScope);
        QTRY_VERIFY(!window->property("settingsSearch").toBool());
        auto *offlineEntry = item("setupOfflineRuleAction");
        QVERIFY(offlineEntry && offlineEntry->isVisible());
        QCOMPARE(offlineEntry->property("text").toString(), QString("Add offline app rule…"));
        clickItem(offlineEntry);
        auto *management = window->findChild<QObject *>("managementDialog");
        QVERIFY(management);
        QTRY_VERIFY(management->property("opened").toBool());
        auto *offlineClass = item("offlineClass");
        QVERIFY(offlineClass && offlineClass->isVisible());
        QVERIFY(QMetaObject::invokeMethod(management, "close"));
        QTRY_VERIFY(!management->property("opened").toBool());
        QTest::qWait(250);
        QTRY_VERIFY(item("windowEntry0"));
        auto *entry = item("windowEntry0");
        QVERIFY(entry->isVisible());
        window->grabWindow();
        clickItem(entry);
        QTRY_VERIFY(!window->property("windowsPage").toBool());
        QCOMPARE(backend.selectedIndex(), 0);
        QCOMPARE(window->property("editingRuleIndex").toInt(), 0);
        QCOMPARE(window->property("editingRuleId").toString(), QString("ui-rule"));
        const QList<QSize> sizes{{860, 680}, {1120, 820}, {1440, 900}};
        for (const auto &size : sizes) {
            window->resize(size);
            for (int section = 0; section < 5; ++section) {
                navigate(section);
                QCOMPARE(window->property("category").toInt(), section);
                QVERIFY(!window->property("windowsPage").toBool());
                QVERIFY(item("editorPage")->isVisible());
                QVERIFY(!item("windowsPage")->isVisible());
                auto *bar = item("ruleActionBar");
                QVERIFY(bar && bar->isVisible());
                if (section == 0) {
                    QVERIFY(item("widthField"));
                    QVERIFY(item("heightField"));
                    for (const auto *fieldName : {"widthField", "heightField", "xField", "yField",
                                                   "minWidthField", "minHeightField", "maxWidthField",
                                                   "maxHeightField"}) {
                        auto *field = item(fieldName);
                        QVERIFY2(field, fieldName);
                        QVERIFY2(field->property("editable").toBool(), fieldName);
                    }
                    QCOMPARE(item("widthField")->property("value").toInt(), 800);
                    QCOMPARE(item("heightField")->property("value").toInt(), 600);
                    QCOMPARE(item("xField")->property("value").toInt(), 135);
                    QCOMPARE(item("yField")->property("value").toInt(), 240);
                    QVERIFY(item("centerBox")->property("checked").toBool() == false);
                    QVERIFY(item("positionBox")->property("checked").toBool());
                    if (size.width() >= 1060) {
                        auto *livePreview = item("livePreviewCard");
                        QVERIFY(livePreview && livePreview->isVisible());
                        QVERIFY(item("previewCanvas"));
                        QVERIFY(item("previewMonitor"));
                        QVERIFY(item("previewWindowShape"));
                        QVERIFY(item("previewGeometrySummary")->property("text").toString().contains("800 × 600"));
                        QVERIFY(item("previewOpacitySummary")->property("text").toString().contains("0.60"));
                        QVERIFY(item("previewChangeSummary"));
                        QVERIFY(item("impactWindowList"));
                        QVERIFY(item("impactFilter_matches")->isVisible());
                        QVERIFY(item("impactFilter_uncertain")->isVisible());
                        QVERIFY(item("impactFilter_unaffected")->isVisible());
                        QVERIFY(item("impactWindowName")->property("text").toString().contains("Terminal"));
                        QVERIFY(item("impactWindowReason")->property("text").toString().contains("All available conditions match"));
                        clickItem(item("impactFilter_unaffected"));
                        QVERIFY(item("impactWindowName")->property("text").toString().contains("Browser"));
                        QVERIFY(item("impactWindowReason")->property("text").toString().contains("does not match"));
                        clickItem(item("impactFilter_matches"));
                        auto *impactSource = item("impactSource");
                        QVERIFY(impactSource && impactSource->isVisible());
                        QVERIFY(impactSource->setProperty("currentIndex", 1));
                        QVERIFY(QMetaObject::invokeMethod(impactSource, "activated", Q_ARG(int, 1)));
                        QVERIFY(window->property("showAppImpact").toBool());
                        QVERIFY(impactSource->setProperty("currentIndex", 0));
                        QVERIFY(QMetaObject::invokeMethod(impactSource, "activated", Q_ARG(int, 0)));
                        QVERIFY(!window->property("showAppImpact").toBool());
                        const QString rulePreviewBefore = item("rulePreviewText")->property("text").toString();
                        auto *widthEditor = qobject_cast<QQuickItem *>(
                            item("widthField")->property("contentItem").value<QObject *>());
                        QVERIFY(widthEditor);
                        QVERIFY(QMetaObject::invokeMethod(widthEditor, "forceActiveFocus"));
                        QTRY_VERIFY(widthEditor->hasActiveFocus());
                        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
                        for (const auto digit : QStringLiteral("811"))
                            QTest::keyClick(window, digit.toLatin1());
                        QTest::keyClick(window, Qt::Key_Return);
                        QTRY_COMPARE(item("widthField")->property("value").toInt(), 811);
                        QTRY_VERIFY(item("previewGeometrySummary")->property("text").toString().contains("811 × 600"));
                        QTRY_VERIFY(item("rulePreviewText")->property("text").toString() != rulePreviewBefore);
                        item("widthField")->setProperty("value", 800);
                        clickItem(item("previewMode_current"));
                        QVERIFY(item("previewOpacitySummary")->property("text").toString().contains("unavailable"));
                        clickItem(item("previewMode_compare"));
                        QVERIFY(item("previewGeometrySummary")->property("text").toString().contains("Current 800 × 600 → Draft 800 × 600"));
                        QVERIFY(item("previewCurrentShape")->isVisible());
                        QVERIFY(item("previewWindowShape")->isVisible());
                        QVERIFY(item("previewCompareLegend")->isVisible());
                        const QString previewScreenshots = qEnvironmentVariable("WINDOWTUNER_TEST_SCREENSHOTS");
                        if (!previewScreenshots.isEmpty())
                            QVERIFY(window->grabWindow().save(previewScreenshots + "/layout-compare-1120x820.png"));
                        clickItem(item("previewMode_draft"));
                    } else {
                        QVERIFY(!item("livePreviewCard")->isVisible());
                        auto *smallPreviewToggle = item("smallPreviewToggle");
                        QVERIFY(smallPreviewToggle && smallPreviewToggle->isVisible());
                        clickItem(smallPreviewToggle);
                        QTRY_VERIFY(item("livePreviewCard")->isVisible());
                        QVERIFY(!item("settingsScroll")->isVisible());
                        QVERIFY(item("previewGeometrySummary")->isVisible());
                        QVERIFY(item("impactFilter_matches")->isVisible());
                        QVERIFY(item("impactFilter_unaffected")->isVisible());
                        const QString narrowPreviewScreenshots = qEnvironmentVariable("WINDOWTUNER_TEST_SCREENSHOTS");
                        if (!narrowPreviewScreenshots.isEmpty())
                            QVERIFY(window->grabWindow().save(narrowPreviewScreenshots + "/layout-preview-860x680.png"));
                        clickItem(smallPreviewToggle);
                        QTRY_VERIFY(item("settingsScroll")->isVisible());
                    }
                }
                if (section == 1) {
                    QCOMPARE(item("workspaceField")->property("text").toString(), QString("2"));
                    QVERIFY(item("pinBox")->property("checked").toBool());
                }
                if (section == 2) {
                    QVERIFY(item("opacityBox")->property("checked").toBool());
                    QCOMPARE(item("activeOpacity")->property("value").toDouble(), 0.6);
                    QCOMPARE(item("inactiveOpacity")->property("value").toDouble(), 0.75);
                    QVERIFY(item("borderBox")->property("checked").toBool());
                    QCOMPARE(item("borderSize")->property("value").toInt(), 5);
                    QVERIFY(item("roundingBox")->property("checked").toBool());
                    QCOMPARE(item("roundingField")->property("value").toInt(), 12);
                }
                if (section == 3) {
                    QVERIFY(item("focusBox")->property("checked").toBool());
                    QVERIFY(item("animationBox")->property("checked").toBool());
                    QVERIFY(item("dimBox")->property("checked").toBool());
                    QVERIFY(item("vrrBox")->property("checked").toBool());
                    QVERIFY(item("persistentSizeBox")->property("checked").toBool());
                }
                if (section == 4)
                    QVERIFY(item("selectedWindowRuleHint")->property("text").toString()
                                .contains("Terminal rule"));
                const QPointF pos = bar->mapToScene(QPointF());
                QVERIFY(pos.y() >= 0);
                QVERIFY(pos.y() + bar->height() <= window->height());
                QVERIFY(pos.x() + bar->width() <= window->width());
            }
            navigate(-2);
            QVERIFY(item("desktopsPage")->isVisible());
            auto *monitorCard = item("desktopMapMonitor0");
            QVERIFY(monitorCard && monitorCard->isVisible());
            QVERIFY(item("desktopMapWorkspace1")->isVisible());
            const QPointF monitorPosition = monitorCard->mapToScene(QPointF());
            QVERIFY(monitorPosition.x() + monitorCard->width() <= window->width());
            const auto screenshots = qEnvironmentVariable("WINDOWTUNER_TEST_SCREENSHOTS");
            if (!screenshots.isEmpty() && size == QSize(860, 680)) {
                QDir().mkpath(screenshots);
                QVERIFY(window->grabWindow().save(screenshots + "/desktops-860x680.png"));
            }
            clickItem(item("desktopScenesAction"));
            auto *scenesPopup = window->findChild<QObject *>("scenesDialog");
            QVERIFY(scenesPopup);
            QTRY_VERIFY(scenesPopup->property("opened").toBool());
            QVERIFY(scenesPopup->property("width").toDouble() <= window->width());
            QVERIFY(scenesPopup->property("height").toDouble() <= window->height());
            auto *scenesScroll = item("scenesScroll");
            QVERIFY(scenesScroll);
            QVERIFY(scenesScroll->property("contentWidth").toDouble()
                    <= scenesScroll->property("availableWidth").toDouble() + 1);
            if (!screenshots.isEmpty() && size == QSize(860, 680))
                QVERIFY(window->grabWindow().save(screenshots + "/scenes-860x680.png"));
            QVERIFY(QMetaObject::invokeMethod(scenesPopup, "close"));
            QTRY_VERIFY(!scenesPopup->property("opened").toBool());
            QTest::qWait(250);
            navigate(0);
        }
        // Every settings module has distinct, nonempty help, not just the
        // first card on each page. Verify the actual hover and focus behavior.
        const QStringList helpKeys{"layout", "size", "constraints", "workspace", "matching",
                                   "opacity", "decoration", "effects", "focus", "performance", "saved"};
        for (const auto &key : helpKeys) {
            auto *heading = findVisual(window->contentItem(), "moduleHelp_" + key);
            QVERIFY2(heading, qPrintable(key));
            QVERIFY(heading->property("explanation").toString().size() > 60);
            QVERIFY(findVisual(heading, "moduleInfoButton"));
        }
        const QStringList firstHelp{"layout", "workspace", "opacity", "focus", "saved"};
        for (int section = 0; section < firstHelp.size(); ++section) {
            navigate(section);
            auto *heading = findVisual(window->contentItem(), "moduleHelp_" + firstHelp[section]);
            auto *info = findVisual(heading, "moduleInfoButton");
            QTest::qWait(200);
            const QPoint point = info->mapToScene(QPointF(info->width() / 2, info->height() / 2)).toPoint();
            QTest::mouseMove(window, point);
            QTRY_VERIFY(heading->property("helpOpen").toBool());
            QTest::mouseMove(window, QPoint(5, 5));
            QTRY_VERIFY(!heading->property("helpOpen").toBool());
            info->forceActiveFocus();
            QTRY_VERIFY(heading->property("helpOpen").toBool());
            item("headerSearch")->forceActiveFocus();
            QTRY_VERIFY(!heading->property("helpOpen").toBool());
        }
        navigate(0);
        QVERIFY(QMetaObject::invokeMethod(item("previewAction"), "clicked"));
        QTRY_VERIFY(item("rulePreviewPanel")->isVisible());
        window->resize(860, 680);
        QCoreApplication::processEvents();
        auto *expandedBar = item("ruleActionBar");
        QTRY_VERIFY(expandedBar->mapToScene(QPointF()).y() + expandedBar->height() <= window->height());
        QTest::keyClick(window, Qt::Key_5, Qt::AltModifier);
        QTRY_COMPARE(window->property("category").toInt(), 4);
        QTest::keyClick(window, Qt::Key_K, Qt::ControlModifier);
        QTRY_VERIFY(item("headerSearch")->hasActiveFocus());
        for (char key : QByteArray("no matching rule")) QTest::keyClick(window, key);
        QTRY_COMPARE(window->property("ruleFilter").toString(), QString("no matching rule"));
        navigate(-1);
        item("headerSearch")->forceActiveFocus();
        for (char key : QByteArray("missing window")) QTest::keyClick(window, key);
        QTRY_COMPARE(window->property("windowFilter").toString(), QString("missing window"));
        QTRY_COMPARE(window->property("visibleWindows").value<QJSValue>().toVariant().toList().size(), 0);
        window->setProperty("windowFilter", "");
        navigate(0);
        // Exercise the real save/edit actions only against disposable config.
        QVERIFY(QMetaObject::invokeMethod(item("saveRuleAction"), "clicked"));
        auto *review = window->findChild<QObject *>("saveReview");
        QVERIFY(review);
        QVERIFY(QMetaObject::invokeMethod(review, "accept"));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(window, "loadRule", Q_ARG(QVariant, 0)));
        QCOMPARE(window->property("editingRuleIndex").toInt(), 0);
        const auto editedId = window->property("editingRuleId").toString();
        QVERIFY(!editedId.isEmpty());
        QVERIFY(backend.duplicateRule(0));
        QVERIFY(backend.moveRule(0, 1));
        QVERIFY(QMetaObject::invokeMethod(window, "requestSave"));
        QCOMPARE(review->property("ruleIndex").toInt(), 1);
        QCOMPARE(backend.savedRules()[1].toMap().value("id").toString(), editedId);
        QVERIFY(QMetaObject::invokeMethod(review, "reject"));
        QVERIFY(backend.deleteRule(0));
        QVERIFY(backend.deleteRule(0));
        QTRY_VERIFY(!window->property("editTargetAvailable").toBool());
        QVERIFY(backend.undo());
        QTRY_VERIFY(window->property("editTargetAvailable").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_COMPARE(window->property("editingRuleIndex").toInt(), -1);
        QCOMPARE(window->property("editingRuleId").toString(), QString());
        QVERIFY(QMetaObject::invokeMethod(window, "chooseWindow", Q_ARG(QVariant, 0)));
        navigate(0);
        item("widthField")->setProperty("value", 811);
        navigate(2);
        item("activeOpacity")->setProperty("value", 0.42);
        QVERIFY(QMetaObject::invokeMethod(item("cancelEditAction"), "clicked"));
        QTRY_COMPARE(window->property("editingRuleIndex").toInt(), -1);
        QCOMPARE(window->property("editingRuleId").toString(), QString());
        navigate(0);
        QCOMPARE(item("widthField")->property("value").toInt(), 800);
        navigate(2);
        QCOMPARE(item("activeOpacity")->property("value").toDouble(), 0.6);
        item("widthField")->setProperty("value", 811);
        QVERIFY(item("cancelEditAction")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(item("cancelEditAction"), "clicked"));
        navigate(0);
        QCOMPARE(item("widthField")->property("value").toInt(), 800);
        const QString screenshots = qEnvironmentVariable("WINDOWTUNER_TEST_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            window->setProperty("previewExpanded", false);
            for (const auto &size : sizes) {
                window->resize(size);
                navigate(0);
                QTest::qWait(150);
                QVERIFY(window->grabWindow().save(screenshots + QString("/layout-%1x%2.png").arg(size.width()).arg(size.height())));
            }
            window->resize(1120, 820);
            navigate(1);
            auto *heading = findVisual(window->contentItem(), "moduleHelp_workspace");
            auto *info = findVisual(heading, "moduleInfoButton");
            QTest::mouseMove(window, info->mapToScene(QPointF(info->width() / 2, info->height() / 2)).toPoint());
            QTRY_VERIFY(heading->property("helpOpen").toBool());
            QTest::qWait(150);
            QVERIFY(window->grabWindow().save(screenshots + "/workspace-help.png"));
        }
        QCOMPARE(warnings.size(), 0);
    }
    void parsesWindowIdentity()
    {
        const auto windows = Backend::parseClients(R"([{"address":"0x123","class":"foot","initialClass":"foot","title":"Terminal","initialTitle":"Terminal","workspace":{"name":"2"},"at":[10,20],"size":[800,600],"pid":1234,"xwayland":false,"fullscreen":0,"pinned":false,"grouped":[],"stableId":"stable-foot","xdgTag":"","contentType":"none","focusHistoryID":0,"floating":true}])");
        QCOMPARE(windows.size(), 1);
        QCOMPARE(windows.first().toMap().value("address").toString(), QString("0x123"));
        QCOMPARE(windows.first().toMap().value("class").toString(), QString("foot"));
        QCOMPARE(windows.first().toMap().value("title").toString(), QString("Terminal"));
        QCOMPARE(windows.first().toMap().value("workspace").toString(), QString("2"));
        QVERIFY(windows.first().toMap().value("floating").toBool());
        QCOMPARE(windows.first().toMap().value("at").toList().at(0).toInt(), 10);
        QCOMPARE(windows.first().toMap().value("size").toList().at(1).toInt(), 600);
        QCOMPARE(windows.first().toMap().value("pid").toInt(), 1234);
        QCOMPARE(windows.first().toMap().value("stableId").toString(), QString("stable-foot"));
    }

    void rejectsMalformedClientData()
    {
        QString error;
        QVERIFY(Backend::parseClients("not-json", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void buildsScopedLuaRule()
    {
        const QVariantMap window{{"class", "foot"}, {"title", "Work [1]"}};
        const QVariantMap settings{{"layout", "floating"}, {"matchTitle", true},
                                   {"center", true}, {"width", 700}, {"height", 500},
                                   {"pin", true}, {"workspace", "3 silent"}};
        const QString rule = Backend::luaRule(window, settings);
        QVERIFY(rule.contains("class = \"^foot$\""));
        QVERIFY(rule.contains("title = \"^Work \\\\[1\\\\]$\""));
        QVERIFY(rule.contains("float = true"));
        QVERIFY(rule.contains("center = true"));
        QVERIFY(rule.contains("size = { 700, 500 }"));
        QVERIFY(rule.contains("workspace = \"3 silent\""));
        const QString live = Backend::liveApplyCode({{"address", "0x123"}}, settings);
        QVERIFY(live.contains("window = \"address:0x123\""));
        QVERIFY(live.contains("hl.dsp.window.resize"));
        QVERIFY(live.contains("hl.dsp.window.center"));
    }

    void buildsAdvancedMatchConditions()
    {
        const QVariantMap window{{"class", "foot"}, {"title", "Settings"}};
        const QVariantMap match{{"class", "foot"}, {"title", "Settings.*"},
                                {"workspace", "2"}, {"xwayland", false}, {"fullscreen", true}};
        const QVariantMap settings{{"match", match}, {"classMatchMode", "contains"},
                                   {"titleMatchMode", "regex"}, {"layout", "keep"}};
        const QString rule = Backend::luaRule(window, settings);
        QVERIFY(rule.contains("class = \".*foot.*\""));
        QVERIFY(rule.contains("title = \"Settings.*\""));
        QVERIFY(rule.contains("workspace = \"2\""));
        QVERIFY(rule.contains("xwayland = false"));
        QVERIFY(rule.contains("fullscreen = true"));
    }

    void savesAndLoadsPersistentRule()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString mainConfig = dir.filePath("hyprland.lua");
        QFile config(mainConfig);
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("-- user config\n");
        config.close();

        const QString fixture = dir.filePath("clients.json");
        QFile clients(fixture);
        QVERIFY(clients.open(QIODevice::WriteOnly));
        clients.write(R"([{"address":"0x1","class":"foot","initialClass":"foot","title":"Work","initialTitle":"Work","workspace":{"name":"1"},"at":[10,20],"size":[800,600],"pid":42,"focusHistoryID":0,"floating":false}])");
        clients.close();

        const QString fakeHyprctl = dir.filePath("hyprctl");
        QFile script(fakeHyprctl);
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nif [ \"$2\" = clients ]; then cat '" + fixture.toUtf8() + "'; exit 0; fi\nexit 0\n");
        script.close();
        QVERIFY(QFile::setPermissions(fakeHyprctl, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        Backend backend(dir.path(), mainConfig, fakeHyprctl);
        QCOMPARE(backend.windows().size(), 1);
        QCOMPARE(backend.savedRules().size(), 0);
        QVERIFY(backend.pickWindowAt(100, 100));
        QCOMPARE(backend.selectedWindow().value("address").toString(), QString("0x1"));
        const QVariantMap settings{{"layout", "floating"}, {"matchTitle", true},
                                   {"center", true}, {"width", 500}, {"height", 500}};
        QVERIFY(backend.saveRule(settings));
        QVERIFY(backend.canUndo());
        QVERIFY(backend.createBackup("Working setup"));
        QVERIFY(backend.savePreset("Compact", settings));
        QVERIFY(!backend.savePreset("Compact", settings));
        const auto presetId = backend.presets().first().toMap().value("id").toString();
        QCOMPARE(backend.presetSettings(presetId).value("width").toInt(), 500);
        QVERIFY(!backend.presetSettings(presetId).contains("matchTitle"));
        QVERIFY(backend.saveProfile("Work"));
        const auto profileId = backend.profiles().first().toMap().value("id").toString();
        QVERIFY(backend.profilePreview(profileId).contains("foot"));
        QVERIFY(backend.reviewChanges(settings, 0).value("moduleAfter").toString().contains("hl.window_rule"));
        QVERIFY(backend.undo());
        QCOMPARE(backend.savedRules().size(), 0);
        QVERIFY(backend.canRedo());
        QVERIFY(backend.redo());
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(backend.tryGeometry(settings, 1));
        QVERIFY(backend.trialActive());
        QTRY_VERIFY_WITH_TIMEOUT(!backend.trialActive(), 2500);
        QFile rules(dir.filePath("windowtuner.lua"));
        QVERIFY(rules.open(QIODevice::ReadOnly));
        const QByteArray lua = rules.readAll();
        QVERIFY(lua.contains("class = \"^foot$\""));
        QVERIFY(lua.contains("title = \"^Work$\""));
        QVERIFY(lua.contains("size = { 500, 500 }"));
        rules.close();
        QVariantMap editable = backend.loadRuleForEditing(0);
        QCOMPARE(editable.value("width").toInt(), 500);
        editable.insert("width", 520);
        QVERIFY(backend.updateRule(0, editable));
        QVERIFY(rules.open(QIODevice::ReadOnly));
        QVERIFY(rules.readAll().contains("size = { 520, 500 }"));
        rules.close();
        QFile updatedConfig(mainConfig);
        QVERIFY(updatedConfig.open(QIODevice::ReadOnly));
        QVERIFY(updatedConfig.readAll().contains("dofile("));
        QVERIFY(QFile::exists(mainConfig + ".windowtuner.bak"));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(QFile::exists(dir.filePath("windowtuner.json")));
        QVERIFY(backend.setRuleEnabled(0, false));
        QCOMPARE(backend.savedRules().at(0).toMap().value("enabled").toBool(), false);
        QVERIFY(backend.setRuleEnabled(0, true));
        QVERIFY(backend.duplicateRule(0));
        QCOMPARE(backend.savedRules().size(), 2);
        QVERIFY(backend.savedRules().at(0).toMap().value("conflict").toBool());
        QCOMPARE(backend.inspectWindow().value("rules").toList().size(), 2);
        QVERIFY(backend.inspectWindow().value("conflicts").toStringList().isEmpty());
        auto conflicting = backend.savedRules().at(1).toMap().value("settings").toMap();
        conflicting.insert("width", 640);
        QVERIFY(backend.updateRule(1, conflicting));
        QVERIFY(!backend.inspectWindow().value("conflicts").toStringList().isEmpty());
        QVERIFY(!backend.inspectWindow().value("conflictMap").toList().isEmpty());
        const QVariantMap conflict = backend.inspectWindow().value("conflictMap").toList().first().toMap();
        QCOMPARE(conflict.value("setting").toString(), QString("width"));
        QVERIFY(!conflict.value("firstRule").toString().isEmpty());
        QVERIFY(!conflict.value("secondRule").toString().isEmpty());
        QVERIFY(!backend.savedRules().at(0).toMap().value("conflictsWith").toList().isEmpty());
        QVERIFY(backend.moveRule(1, 0));
        QVERIFY(backend.applySavedRule(0));
        QVERIFY(backend.exportRules().contains("\"class\": \"foot\""));
        QVERIFY(!backend.importRules("not-json"));
        QVERIFY(backend.importRules(backend.exportRules()));
        QCOMPARE(backend.savedRules().size(), 2);
        QFile failingHyprctl(fakeHyprctl);
        QVERIFY(failingHyprctl.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QString ruleData = dir.filePath("windowtuner.json");
        failingHyprctl.write("#!/bin/sh\nif [ \"$1\" = -j ] && [ \"$2\" = clients ]; then cat '" + fixture.toUtf8() +
            "'; exit 0; fi\nif [ \"$1\" = configerrors ]; then "
            "if [ -f '" + ruleData.toUtf8() + "' ] && grep -q '\"enabled\": false' '" + ruleData.toUtf8() + "'; "
            "then echo 'invalid test config'; else echo '[]'; fi; "
            "fi\nexit 0\n");
        failingHyprctl.close();
        QVERIFY(QFile::setPermissions(fakeHyprctl, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(!backend.setRuleEnabled(0, false));
        QVERIFY(backend.savedRules().at(0).toMap().value("enabled").toBool());
        QVERIFY(failingHyprctl.open(QIODevice::WriteOnly | QIODevice::Truncate));
        failingHyprctl.write("#!/bin/sh\nif [ \"$2\" = clients ]; then cat '" + fixture.toUtf8() + "'; exit 0; fi\nexit 0\n");
        failingHyprctl.close();
        QVERIFY(QFile::setPermissions(fakeHyprctl, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(backend.deleteRule(0));
        QVERIFY(backend.activateProfile(profileId));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(backend.removeLibraryItem("presets", presetId));
        QCOMPARE(backend.presets().size(), 0);
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(rules.open(QIODevice::ReadOnly));
        QCOMPARE(rules.readAll().count("hl.window_rule("), 1);
        rules.close();
        QVERIFY(backend.restoreBackup(backend.backups().first().toMap().value("id").toString()));
        QCOMPARE(backend.savedRules().size(), 1);
        updatedConfig.close();
        QVERIFY(updatedConfig.open(QIODevice::WriteOnly | QIODevice::Append));
        updatedConfig.write("-- external edit\n");
        updatedConfig.close();
        QVERIFY(!backend.deleteRule(0));
        QVERIFY(backend.status().contains("outside"));
        QVERIFY(backend.reloadFromDisk());
        QVERIFY(backend.deleteRule(0));
    }
};

QTEST_MAIN(WindowTunerTests)
#include "windowtuner_tests.moc"
