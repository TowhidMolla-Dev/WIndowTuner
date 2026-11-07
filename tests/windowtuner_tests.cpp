#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"

class WindowTunerTests : public QObject {
    Q_OBJECT

private slots:
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
        QVERIFY(rule.contains("class = \"foot\""));
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
        QVERIFY(updatedConfig.readAll().contains("require(\"hypr.windowtuner\")"));
        QVERIFY(QFile::exists(mainConfig + ".windowtuner.bak"));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(QFile::exists(dir.filePath("windowtuner.json")));
        QVERIFY(backend.setRuleEnabled(0, false));
        QCOMPARE(backend.savedRules().at(0).toMap().value("enabled").toBool(), false);
        QVERIFY(backend.setRuleEnabled(0, true));
        QVERIFY(backend.duplicateRule(0));
        QCOMPARE(backend.savedRules().size(), 2);
        QVERIFY(backend.savedRules().at(0).toMap().value("conflict").toBool());
        QVERIFY(!backend.savedRules().at(0).toMap().value("conflictsWith").toList().isEmpty());
        QVERIFY(backend.moveRule(1, 0));
        QVERIFY(backend.applySavedRule(0));
        QVERIFY(backend.exportRules().contains("\"class\": \"foot\""));
        QVERIFY(!backend.importRules("not-json"));
        QVERIFY(backend.importRules(backend.exportRules()));
        QCOMPARE(backend.savedRules().size(), 2);
        QFile failingHyprctl(fakeHyprctl);
        QVERIFY(failingHyprctl.open(QIODevice::WriteOnly | QIODevice::Truncate));
        failingHyprctl.write("#!/bin/sh\nif [ \"$1\" = -j ] && [ \"$2\" = clients ]; then cat '" + fixture.toUtf8() + "'; exit 0; fi\nif [ \"$1\" = configerrors ]; then echo 'invalid test config'; exit 0; fi\nexit 0\n");
        failingHyprctl.close();
        QVERIFY(QFile::setPermissions(fakeHyprctl, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(!backend.setRuleEnabled(0, false));
        QVERIFY(backend.savedRules().at(0).toMap().value("enabled").toBool());
        QVERIFY(failingHyprctl.open(QIODevice::WriteOnly | QIODevice::Truncate));
        failingHyprctl.write("#!/bin/sh\nif [ \"$2\" = clients ]; then cat '" + fixture.toUtf8() + "'; exit 0; fi\nexit 0\n");
        failingHyprctl.close();
        QVERIFY(QFile::setPermissions(fakeHyprctl, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(backend.deleteRule(0));
        QCOMPARE(backend.savedRules().size(), 1);
        QVERIFY(rules.open(QIODevice::ReadOnly));
        QCOMPARE(rules.readAll().count("o.window("), 1);
    }
};

QTEST_GUILESS_MAIN(WindowTunerTests)
#include "windowtuner_tests.moc"
