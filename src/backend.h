#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QLocalSocket>
#include <QJsonArray>
#include <QJsonObject>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString rulePath READ rulePath CONSTANT)
    Q_PROPERTY(QVariantList savedRules READ savedRules NOTIFY savedRulesChanged)
    Q_PROPERTY(QVariantMap selectedWindow READ selectedWindow NOTIFY selectedChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY safetyChanged)
    Q_PROPERTY(QVariantList backups READ backups NOTIFY safetyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY safetyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY safetyChanged)
    Q_PROPERTY(bool trialActive READ trialActive NOTIFY safetyChanged)
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY windowsChanged)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY libraryChanged)
    Q_PROPERTY(QString configEntryPoint READ configEntryPoint NOTIFY healthChanged)
    Q_PROPERTY(QString lastSuccessfulSave READ lastSuccessfulSave NOTIFY healthChanged)
    Q_PROPERTY(QVariantList applicationCatalog READ applicationCatalog NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList workspacePlan READ workspacePlan NOTIFY workspacePlanChanged)
    Q_PROPERTY(QVariantList monitorPlan READ monitorPlan NOTIFY workspacePlanChanged)
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList sessionSnapshots READ sessionSnapshots NOTIFY libraryChanged)
    Q_PROPERTY(bool sessionSnapshotsEnabled READ sessionSnapshotsEnabled NOTIFY libraryChanged)
    Q_PROPERTY(bool monitorRevertPending READ monitorRevertPending NOTIFY monitorRevertChanged)
    Q_PROPERTY(QString appSpacePath READ appSpacePath CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;
    Backend(const QString &configDirectory, const QString &hyprlandConfig,
            const QString &hyprctl, QObject *parent = nullptr);

    QVariantList windows() const { return m_windows; }
    int selectedIndex() const { return m_selectedIndex; }
    QString status() const { return m_status; }
    QString rulePath() const { return m_rulePath; }
    QVariantList savedRules() const { return m_savedRules; }
    QVariantMap selectedWindow() const { return !m_offlineTarget.isEmpty() ? m_offlineTarget : m_selectedIndex >= 0 && m_selectedIndex < m_windows.size() ? m_windows.at(m_selectedIndex).toMap() : QVariantMap{}; }

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
    Q_INVOKABLE QVariantMap editorStateForWindow(int index) const;
    Q_INVOKABLE bool applySavedRule(int index);
    Q_INVOKABLE QString exportRules() const;
    Q_INVOKABLE bool importRules(const QString &json);
    static QString liveApplyCode(const QVariantMap &window, const QVariantMap &settings);

    static QVariantList parseClients(const QByteArray &json, QString *error = nullptr);
    static QString luaRule(const QVariantMap &window, const QVariantMap &settings,
                           const QJsonObject &workspaces = {});
    QVariantList history() const;
    QVariantList backups() const;
    bool canUndo() const { return !m_past.isEmpty(); }
    bool canRedo() const { return !m_future.isEmpty(); }
    bool trialActive() const { return !m_trialWindow.isEmpty(); }
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE bool createBackup(const QString &name);
    Q_INVOKABLE bool restoreBackup(const QString &id);
    Q_INVOKABLE bool reloadFromDisk();
    Q_INVOKABLE QVariantMap reviewChanges(const QVariantMap &settings, int index = -1) const;
    Q_INVOKABLE QStringList validateSettings(const QVariantMap &settings) const;
    Q_INVOKABLE QVariantMap validationDetails(const QVariantMap &settings) const;
    Q_INVOKABLE QString summarize(const QVariantMap &settings) const;
    Q_INVOKABLE bool tryGeometry(const QVariantMap &settings, int seconds = 15);
    Q_INVOKABLE bool revertTrial();
    QVariantList monitors() const { return m_monitors; }
    Q_INVOKABLE QVariantMap resolvePlacement(const QVariantMap &settings) const;
    static QVariantMap placementGeometry(const QVariantMap &placement, const QVariantList &monitors);
    static QString ruleModule(const QJsonArray &rules, const QJsonObject &workspaces = {},
                              const QJsonObject &monitors = {});
    static QVariantMap matchWindow(const QVariantMap &window, const QVariantMap &settings);
    Q_INVOKABLE QVariantMap testMatch(const QVariantMap &settings) const;
    Q_INVOKABLE QVariantMap simulateRule(const QVariantMap &settings) const;
    Q_INVOKABLE QVariantMap inspectWindow() const;
    Q_INVOKABLE QVariantList externalRuleHints() const;
    QVariantList presets() const;
    QVariantList profiles() const;
    Q_INVOKABLE bool savePreset(const QString &name, const QVariantMap &settings);
    Q_INVOKABLE QVariantMap presetSettings(const QString &id) const;
    Q_INVOKABLE bool saveProfile(const QString &name);
    Q_INVOKABLE QString profilePreview(const QString &id) const;
    Q_INVOKABLE bool activateProfile(const QString &id);
    Q_INVOKABLE bool removeLibraryItem(const QString &kind, const QString &id);
    Q_INVOKABLE QVariantList installedApplications() const;
    Q_INVOKABLE bool selectOfflineTarget(const QString &klass, const QString &title = QString());
    Q_INVOKABLE bool setRuleMetadata(const QString &id, const QString &name, const QString &folder, const QString &tags);
    Q_INVOKABLE bool batchSetEnabled(const QVariantList &ids, bool enabled);
    Q_INVOKABLE QString exportSelected(const QVariantList &ids) const;
    Q_INVOKABLE QVariantMap previewImport(const QString &json) const;
    Q_INVOKABLE bool importSelection(const QString &json, const QVariantList &indexes, const QString &mode);
    QString configEntryPoint() const { return m_hyprlandConfig; }
    QString lastSuccessfulSave() const { return m_lastSuccessfulSave; }
    Q_INVOKABLE bool configureEntryPoint(const QString &path);
    Q_INVOKABLE QVariantMap healthReport() const;
    Q_INVOKABLE QVariantMap verifyDraft(const QVariantMap &settings) const;
    Q_INVOKABLE QVariantList auditCapabilities() const;
    Q_INVOKABLE QString diagnosticReport() const;
    Q_INVOKABLE QVariant preference(const QString &key, const QVariant &fallback) const;
    Q_INVOKABLE bool setPreference(const QString &key, const QVariant &value);

    // Where apps open: a merged running/installed catalogue, workspace pins and
    // per-display layout, all generated into the same validated module.
    QVariantList applicationCatalog() const;
    QVariantMap applicationSettings(const QString &klass) const;
    Q_INVOKABLE bool setApplicationPlacement(const QString &klass, const QVariantMap &placement);
    Q_INVOKABLE bool clearApplicationPlacement(const QString &klass);
    QVariantList workspacePlan() const;
    Q_INVOKABLE bool setWorkspacePlan(const QString &slot, const QString &monitor, bool persistent);
    Q_INVOKABLE bool removeWorkspacePlan(const QString &slot);
    QVariantList monitorPlan() const;
    Q_INVOKABLE bool applyMonitorLayout(const QString &description, const QVariantMap &fields);
    Q_INVOKABLE bool keepMonitorLayout();
    Q_INVOKABLE bool revertMonitorLayout();
    bool monitorRevertPending() const { return m_monitorRevertPending; }
    QString appSpacePath() const;
    Q_INVOKABLE QVariantMap appSpaceImportPreview(const QString &path = QString()) const;
    Q_INVOKABLE bool importAppSpace(const QString &path = QString());
    QVariantList scenes() const;
    Q_INVOKABLE bool captureScene(const QString &name);
    Q_INVOKABLE QVariantMap previewScene(const QString &id) const;
    Q_INVOKABLE bool openScene(const QString &id);
    Q_INVOKABLE bool removeScene(const QString &id);
    QVariantList sessionSnapshots() const;
    bool sessionSnapshotsEnabled() const;
    Q_INVOKABLE bool setSessionSnapshotsEnabled(bool enabled);
    Q_INVOKABLE QVariantMap previewSessionSnapshot(const QString &id) const;
    Q_INVOKABLE bool restoreSessionSnapshot(const QString &id);
    static QString deriveApplicationClass(const QString &desktopId, const QString &startupWMClass,
                                          const QString &exec, const QString &browser);
    static QString desktopExecCommand(const QString &exec);
    static QString desktopLaunchTarget(const QString &desktopId, const QString &exec);

signals:
    void windowsChanged();
    void selectedChanged();
    void statusChanged();
    void savedRulesChanged();
    void safetyChanged();
    void libraryChanged();
    void healthChanged();
    void catalogChanged();
    void workspacePlanChanged();
    void monitorRevertChanged();

private:
    void connectHyprlandEvents();
    void saveSessionSnapshot();
    QVariantMap resolveSessionSnapshot(const QJsonObject &snapshot) const;
    bool applyResolvedScene(const QJsonObject &scene, const QVariantMap &resolved);

private slots:
    void handleHyprlandEvent();

private:
    void setStatus(const QString &status);
    bool writeRules(const QJsonArray &rules);
    bool writeRuleData(const QJsonArray &rules);
    bool readRuleData(QJsonArray *rules, QString *error = nullptr) const;
    bool commitRuleData(const QJsonArray &rules, bool ensureInclude = false, bool recordHistory = true);
    void updateSavedRules(const QJsonArray &rules);
    bool ensureMainConfigIncludesRules();
    QString moduleInclude() const;
    QByteArray fingerprint() const;
    QJsonObject snapshot(const QString &name) const;
    bool restoreSnapshot(const QJsonObject &state);
    bool saveJournal();
    void loadJournal();
    void loadLibrary();
    bool writeLibrary(const QJsonObject &library);
    QVariantMap resolveScene(const QJsonObject &scene) const;
    bool decodeRules(const QString &json, QJsonArray *rules, QString *error) const;
    QJsonObject workspaceData() const;
    QJsonObject monitorData() const;
    bool commitPlacementData(const QJsonObject &workspaces, const QJsonObject &monitors,
                             const QString &status, bool keepHistory = true,
                             const QJsonArray &rules = {});
    static QJsonObject appSpaceModuleConfig(const QJsonArray &rules, const QJsonObject &workspaces,
                                            const QJsonObject &monitors);
    QString appSpaceImportPath(const QString &path) const;

    QString m_configDirectory;
    QString m_hyprlandConfig;
    QString m_hyprctl;
    QString m_rulePath;
    QVariantList m_windows;
    QVariantList m_savedRules;
    int m_selectedIndex = -1;
    QString m_status;
    QTimer m_refreshTimer;
    QTimer m_sessionSnapshotTimer;
    QLocalSocket m_eventSocket;
    QByteArray m_eventBuffer;
    bool m_hasRefreshed = false;
    QByteArray m_diskFingerprint;
    QJsonArray m_past, m_future, m_backups;
    QByteArray m_journalBytes;
    bool m_replaying = false;
    QTimer m_trialTimer;
    QVariantMap m_trialWindow;
    QVariantList m_monitors;
    QJsonObject m_library;
    QByteArray m_libraryBytes;
    QVariantMap m_offlineTarget;
    QString m_lastSuccessfulSave;
    QTimer m_monitorRevertTimer;
    QJsonObject m_monitorRevertState;
    bool m_monitorRevertPending = false;
};
