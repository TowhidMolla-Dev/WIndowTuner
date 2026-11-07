#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
public:
    explicit Theme(QObject *parent = nullptr);
    QString accent() const { return m_accent; }
signals:
    void changed();
private:
    void reload();
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_directory;
    QString m_accent = "#FFD60A";
};
