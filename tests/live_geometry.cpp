#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"

class LiveGeometryTests : public QObject {
    Q_OBJECT
private slots:
    void trialAndRevertOnlyTheOwnedWindow()
    {
        const auto ctl = QStandardPaths::findExecutable("hyprctl");
        QVERIFY2(!ctl.isEmpty(), "Run this test inside a Hyprland session.");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile config(dir.filePath("hyprland.lua"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("-- isolated live test; never loaded into Hyprland\n");
        config.close();
        QQuickWindow window;
        window.setTitle("WindowTuner owned geometry test");
        window.setColor(QColor("#15212a"));
        window.resize(800, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        Backend backend(dir.path(), config.fileName(), ctl);
        const auto selectOwned = [&]() {
            backend.refreshWindows();
            const auto clients = backend.windows();
            for (int i = 0; i < clients.size(); ++i) {
                const auto row = clients[i].toMap();
                if (row.value("pid").toLongLong() != QCoreApplication::applicationPid() ||
                    row.value("currentTitle").toString() != window.title()) continue;
                backend.selectWindow(i);
                return true;
            }
            return false;
        };
        QTRY_VERIFY_WITH_TIMEOUT(selectOwned(), 5000);
        // A freshly mapped window can briefly be missing geometry in the
        // compositor's client list. Wait for a usable snapshot before the trial.
        const auto ready = [&]() {
            backend.refreshWindows();
            const auto selected = backend.selectedWindow();
            return !selected.value("address").toString().isEmpty() &&
                selected.value("at").toList().size() == 2 &&
                selected.value("size").toList().size() == 2 &&
                selected.value("fullscreen").toInt() == 0;
        };
        bool usable = false;
        for (int attempt = 0; attempt < 50 && !usable; ++attempt) {
            usable = ready();
            if (!usable) QTest::qWait(100);
        }
        if (!usable) {
            // Some compositor plugins (an overview/dock, for example) take over
            // newly opened windows and report them as fullscreen on a workspace
            // of their choosing. That is session state, not a WindowTuner
            // result, and geometry cannot be observed through it: say so
            // instead of reporting a pass or blaming the app.
            const auto selected = backend.selectedWindow();
            const QByteArray state = QJsonDocument::fromVariant(selected).toJson(QJsonDocument::Compact);
            if (selected.value("fullscreen").toInt() != 0)
                QSKIP(qPrintable(QString("The compositor reports the test window as fullscreen (%1), so "
                    "geometry placement cannot be observed. Close any overview/dock plugin or leave "
                    "a fullscreen workspace and re-run.").arg(QString::fromUtf8(state))));
            QFAIL(qPrintable(QString("The compositor never reported usable geometry for the test "
                "window: %1").arg(QString::fromUtf8(state))));
        }
        const auto original = backend.selectedWindow();
        const auto address = original.value("address").toString();
        QVERIFY(!address.isEmpty());
        const auto currentOwned = [&]() {
            backend.refreshWindows();
            for (const auto &entry : backend.windows())
                if (entry.toMap().value("address").toString() == address) return entry.toMap();
            return QVariantMap{};
        };
        // Never save, import, reload or apply an application-wide rule here.
        // tryGeometry uses the selected address and excludes all appearance,
        // workspace and monitor overrides from its command.
        const QVariantMap trial{{"layout", "floating"}, {"width", 550}, {"height", 360}, {"center", true}};
        bool started = false;
        for (int attempt = 0; attempt < 3 && !started; ++attempt) {
            if (attempt) {
                QTest::qWait(250);
                QVERIFY(selectOwned());
                QTRY_VERIFY_WITH_TIMEOUT(ready(), 3000);
            }
            started = backend.tryGeometry(trial, 30);
        }
        QVERIFY2(started, qPrintable(backend.status() +
            QString(" (%1 open client(s), %2 selected)").arg(backend.windows().size()).arg(backend.selectedWindow().size())));
        QTRY_VERIFY_WITH_TIMEOUT(currentOwned().value("floating").toBool(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(currentOwned().value("size").toList(), (QVariantList{550, 360}), 3000);
        QVERIFY2(backend.revertTrial(), qPrintable(backend.status()));
        QTRY_COMPARE_WITH_TIMEOUT(currentOwned().value("floating").toBool(), original.value("floating").toBool(), 3000);
        QVERIFY(!backend.trialActive());
        QVERIFY(!QFile::exists(dir.filePath("windowtuner.json")));
        QVERIFY(!QFile::exists(dir.filePath("windowtuner.lua")));
        QVERIFY(config.open(QIODevice::ReadOnly));
        QCOMPARE(config.readAll(), QByteArray("-- isolated live test; never loaded into Hyprland\n"));
        window.close();
    }
};

QTEST_MAIN(LiveGeometryTests)
#include "live_geometry.moc"
