#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString rulePath READ rulePath CONSTANT)
    Q_PROPERTY(QVariantList savedRules READ savedRules NOTIFY savedRulesChanged)
    Q_PROPERTY(QVariantMap selectedWindow READ selectedWindow NOTIFY selectedChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    Backend(const QString &configDirectory, const QString &hyprlandConfig,
            const QString &hyprctl, QObject *parent = nullptr);

    QVariantList windows() const { return m_windows; }
    int selectedIndex() const { return m_selectedIndex; }
    QString status() const { return m_status; }
    QString rulePath() const { return m_rulePath; }
    QVariantList savedRules() const { return m_savedRules; }
    QVariantMap selectedWindow() const { return m_selectedIndex >= 0 && m_selectedIndex < m_windows.size() ? m_windows.at(m_selectedIndex).toMap() : QVariantMap{}; }

    Q_INVOKABLE void refreshWindows();
    Q_INVOKABLE void selectWindow(int index);
    Q_INVOKABLE bool pickWindowAt(int x, int y);
    Q_INVOKABLE QString previewRule(const QVariantMap &settings) const;
    Q_INVOKABLE bool saveRule(const QVariantMap &settings);
    Q_INVOKABLE bool deleteRule(int index);
    Q_INVOKABLE bool setRuleEnabled(int index, bool enabled);
    Q_INVOKABLE bool duplicateRule(int index);
    Q_INVOKABLE bool moveRule(int index, int destination);
    Q_INVOKABLE bool updateRule(int index, const QVariantMap &settings);
    Q_INVOKABLE QVariantMap loadRuleForEditing(int index);
    Q_INVOKABLE bool applySavedRule(int index);
    Q_INVOKABLE QString exportRules() const;
    Q_INVOKABLE bool importRules(const QString &json);
    static QString liveApplyCode(const QVariantMap &window, const QVariantMap &settings);

    static QVariantList parseClients(const QByteArray &json, QString *error = nullptr);
    static QString luaRule(const QVariantMap &window, const QVariantMap &settings);

signals:
    void windowsChanged();
    void selectedChanged();
    void statusChanged();
    void savedRulesChanged();

private:
    void setStatus(const QString &status);
    bool writeRules(const QJsonArray &rules);
    bool writeRuleData(const QJsonArray &rules);
    bool readRuleData(QJsonArray *rules, QString *error = nullptr) const;
    bool commitRuleData(const QJsonArray &rules, bool ensureInclude = false);
    void updateSavedRules(const QJsonArray &rules);
    bool ensureMainConfigIncludesRules();

    QString m_configDirectory;
    QString m_hyprlandConfig;
    QString m_hyprctl;
    QString m_rulePath;
    QVariantList m_windows;
    QVariantList m_savedRules;
    int m_selectedIndex = -1;
    QString m_status;
    QTimer m_refreshTimer;
    bool m_hasRefreshed = false;
};
