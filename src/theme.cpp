#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

Theme::Theme(QObject *parent)
    : QObject(parent), m_directory(QDir::homePath() + "/.local/state/omarchy/current")
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);
    reload();
}

void Theme::reload()
{
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) m_watcher.removePaths(watched);
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    for (const QString &path : {ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                                m_directory + "/theme", m_directory + "/theme/colors.toml"})
        if (QFileInfo::exists(path)) m_watcher.addPath(path);

    QString accent = "#FFD60A";
    QFile colors(m_directory + "/theme/colors.toml");
    if (colors.open(QIODevice::ReadOnly)) {
        const auto match = QRegularExpression(R"re(^\s*accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                              QRegularExpression::MultilineOption)
                               .match(QString::fromUtf8(colors.readAll()));
        if (match.hasMatch()) accent = match.captured(1);
    }
    if (accent != m_accent) {
        m_accent = accent;
        emit changed();
    }
}
