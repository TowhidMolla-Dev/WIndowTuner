#include "backend.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace {
QByteArray readLibrary(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
}

void Backend::loadLibrary()
{
    m_libraryBytes = readLibrary(m_configDirectory + "/windowtuner-library.json");
    m_library = QJsonDocument::fromJson(m_libraryBytes).object();
}

bool Backend::writeLibrary(const QJsonObject &library)
{
    QDir().mkpath(m_configDirectory);
    QLockFile lock(m_configDirectory + "/windowtuner-library.lock");
    if (!lock.tryLock(0)) { setStatus(tr("Another operation is updating the library.")); return false; }
    const QString path = m_configDirectory + "/windowtuner-library.json";
    if (readLibrary(path) != m_libraryBytes ||
        (!m_libraryBytes.isEmpty() && !QJsonDocument::fromJson(m_libraryBytes).isObject())) {
        setStatus(tr("Library changed externally or is invalid. Restart WindowTuner before changing it."));
        return false;
    }
    const auto data = QJsonDocument(library).toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        setStatus(tr("Could not save preset/profile library.")); return false;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    m_library = library;
    m_libraryBytes = data;
    emit libraryChanged();
    return true;
}

QVariantList Backend::scenes() const
{
    QVariantList result;
    for (const QJsonValue &value : m_library.value("scenes").toArray()) {
        const QJsonObject scene = value.toObject();
        result.append(QVariantMap{
            {"id", scene.value("id").toString()},
            {"name", scene.value("name").toString()},
            {"monitor", scene.value("monitor").toString()},
            {"workspace", scene.value("workspace").toString()},
            {"windowCount", scene.value("windows").toArray().size()},
            {"createdAt", scene.value("createdAt").toString()}
        });
    }
    return result;
}

bool Backend::captureScene(const QString &rawName)
{
    const QString name = rawName.trimmed().left(80);
    if (name.isEmpty()) {
        setStatus(tr("Give this scene a name."));
        return false;
    }
    QJsonArray savedScenes = m_library.value("scenes").toArray();
    for (const QJsonValue &value : savedScenes) {
        if (value.toObject().value("name").toString().compare(name, Qt::CaseInsensitive) == 0) {
            setStatus(tr("A scene with that name already exists."));
            return false;
        }
    }

    QVariantMap activeMonitor;
    bool foundFocused = false;
    for (const QVariant &value : m_monitors) {
        const QVariantMap monitor = value.toMap();
        if (monitor.value("focused").toBool()) {
            activeMonitor = monitor;
            foundFocused = true;
            break;
        }
    }
    if (!foundFocused && m_monitors.size() == 1)
        activeMonitor = m_monitors.first().toMap();
    if (activeMonitor.isEmpty() || activeMonitor.value("description").toString().trimmed().isEmpty()) {
        setStatus(tr("Cannot capture a scene: the active display identity is unavailable."));
        return false;
    }
    const QVariantMap activeWorkspace = activeMonitor.value("activeWorkspace").toMap();
    const QString workspace = activeWorkspace.value("name").toString().trimmed();
    if (workspace.isEmpty() || workspace.startsWith("special:")) {
        setStatus(tr("Cannot capture a scene from this special or unidentified workspace."));
        return false;
    }
    const int monitorId = activeMonitor.value("id", -1).toInt();
    const int monitorX = activeMonitor.value("x", 0).toInt();
    const int monitorY = activeMonitor.value("y", 0).toInt();
    const int monitorWidth = activeMonitor.value("width").toInt();
    const int monitorHeight = activeMonitor.value("height").toInt();
    if (monitorId < 0 || monitorWidth <= 0 || monitorHeight <= 0) {
        setStatus(tr("Cannot capture a scene: monitor geometry is incomplete."));
        return false;
    }

    QList<QVariantMap> windows;
    int skipped = 0;
    for (const QVariant &value : m_windows) {
        const QVariantMap window = value.toMap();
        if (window.value("monitor").toInt() != monitorId ||
            window.value("workspace").toString() != workspace)
            continue;
        const auto at = window.value("at").toList();
        const auto size = window.value("size").toList();
        if (window.value("class").toString().trimmed().isEmpty() ||
            window.value("address").toString().isEmpty() ||
            window.value("fullscreen").toInt() != 0 ||
            at.size() != 2 || size.size() != 2 ||
            at[0].toInt() < 0 || at[1].toInt() < 0 ||
            size[0].toInt() <= 0 || size[1].toInt() <= 0) {
            ++skipped;
            continue;
        }
        windows.append(window);
    }
    if (windows.isEmpty()) {
        setStatus(skipped > 0
            ? tr("No eligible windows to capture; fullscreen or unidentified windows were skipped.")
            : tr("There are no open windows on the active workspace to capture."));
        return false;
    }
    if (windows.size() > 32) {
        setStatus(tr("This scene has too many windows to capture safely (maximum 32)."));
        return false;
    }
    std::sort(windows.begin(), windows.end(), [](const QVariantMap &left, const QVariantMap &right) {
        const auto a = left.value("at").toList();
        const auto b = right.value("at").toList();
        return a[0].toInt() == b[0].toInt()
            ? a[1].toInt() < b[1].toInt()
            : a[0].toInt() < b[0].toInt();
    });

    QJsonArray captured;
    for (const QVariantMap &window : windows) {
        const auto at = window.value("at").toList();
        const auto size = window.value("size").toList();
        const double width = qBound(0.05, size[0].toDouble() / monitorWidth, 1.0);
        const double height = qBound(0.05, size[1].toDouble() / monitorHeight, 1.0);
        const double x = qBound(0.0, (at[0].toDouble() - monitorX) / monitorWidth, 1.0 - width);
        const double y = qBound(0.0, (at[1].toDouble() - monitorY) / monitorHeight, 1.0 - height);
        captured.append(QJsonObject{
            {"class", window.value("class").toString()},
            {"floating", window.value("floating").toBool()},
            {"rect", QJsonObject{{"x", x}, {"y", y}, {"width", width}, {"height", height}}}
        });
    }

    QJsonObject scene{
        {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"name", name},
        {"monitor", activeMonitor.value("description").toString()},
        {"workspace", workspace},
        {"createdAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"windows", captured}
    };
    savedScenes.append(scene);
    QJsonObject library = m_library;
    library.insert("scenes", savedScenes);
    if (!writeLibrary(library)) return false;
    setStatus(skipped > 0
        ? tr("Scene saved with %1 window(s); %2 unsupported window(s) were skipped.").arg(captured.size()).arg(skipped)
        : tr("Scene “%1” saved with %2 window(s).").arg(name).arg(captured.size()));
    return true;
}

QVariantMap Backend::resolveScene(const QJsonObject &scene) const
{
    const QString monitorDescription = scene.value("monitor").toString();
    const QString workspace = scene.value("workspace").toString();
    QVariantMap targetMonitor;
    for (const QVariant &value : m_monitors) {
        const QVariantMap monitor = value.toMap();
        if (monitor.value("description").toString() == monitorDescription) {
            targetMonitor = monitor;
            break;
        }
    }
    if (targetMonitor.isEmpty())
        return {{"error", tr("The scene’s display is not connected. No windows were changed.")}};
    if (workspace.isEmpty() || workspace.startsWith("special:"))
        return {{"error", tr("This scene has an invalid workspace. No windows were changed.")}};

    QVariantList candidates;
    for (const QVariant &value : m_windows) {
        const QVariantMap window = value.toMap();
        if (window.value("address").toString().isEmpty() ||
            window.value("class").toString().isEmpty() ||
            window.value("fullscreen").toInt() != 0 ||
            window.value("workspace").toString().startsWith("special:") ||
            window.value("at").toList().size() != 2 ||
            window.value("size").toList().size() != 2)
            continue;
        candidates.append(window);
    }
    std::sort(candidates.begin(), candidates.end(), [](const QVariant &leftValue, const QVariant &rightValue) {
        const QVariantMap left = leftValue.toMap(), right = rightValue.toMap();
        const auto a = left.value("at").toList(), b = right.value("at").toList();
        return a[0].toInt() == b[0].toInt()
            ? a[1].toInt() < b[1].toInt()
            : a[0].toInt() < b[0].toInt();
    });

    QSet<QString> usedAddresses;
    QVariantList matches;
    QVariantList items;
    int missing = 0;
    const QJsonArray savedWindows = scene.value("windows").toArray();
    if (savedWindows.isEmpty() || savedWindows.size() > 32)
        return {{"error", tr("This scene is empty or exceeds the safe window limit.")}};
    for (const QJsonValue &value : savedWindows) {
        const QJsonObject saved = value.toObject();
        const QString klass = saved.value("class").toString();
        QVariantMap match;
        if (!klass.isEmpty()) {
            for (const QVariant &candidateValue : candidates) {
                const QVariantMap candidate = candidateValue.toMap();
                const QString address = candidate.value("address").toString();
                if (!usedAddresses.contains(address) &&
                    candidate.value("class").toString().compare(klass, Qt::CaseInsensitive) == 0) {
                    match = candidate;
                    usedAddresses.insert(address);
                    break;
                }
            }
        }
        if (match.isEmpty()) {
            ++missing;
            items.append(QVariantMap{{"class", klass}, {"matched", false}});
        } else {
            matches.append(QVariantMap{{"saved", saved.toVariantMap()}, {"window", match}});
            items.append(QVariantMap{{"class", klass}, {"matched", true}});
        }
    }
    return {{"items", items}, {"matches", matches}, {"matched", matches.size()},
        {"missing", missing}, {"monitor", monitorDescription}, {"workspace", workspace},
        {"targetMonitor", targetMonitor}};
}

QVariantMap Backend::previewScene(const QString &id) const
{
    for (const QJsonValue &value : m_library.value("scenes").toArray()) {
        const QJsonObject scene = value.toObject();
        if (scene.value("id").toString() != id) continue;
        QVariantMap result = resolveScene(scene);
        result.insert("id", id);
        result.insert("name", scene.value("name").toString());
        result.insert("previewOnly", true);
        result.remove("matches");
        result.remove("targetMonitor");
        return result;
    }
    return {{"error", tr("Scene not found.")}, {"matched", 0}, {"missing", 0}, {"previewOnly", true}};
}

bool Backend::openScene(const QString &id)
{
    if (trialActive() || m_monitorRevertPending) {
        setStatus(tr("Finish or revert the current display/geometry trial before opening a scene."));
        return false;
    }
    QJsonObject scene;
    for (const QJsonValue &value : m_library.value("scenes").toArray())
        if (value.toObject().value("id").toString() == id) { scene = value.toObject(); break; }
    if (scene.isEmpty()) { setStatus(tr("Scene not found.")); return false; }
    const QVariantMap resolved = resolveScene(scene);
    if (!resolved.value("error").toString().isEmpty()) {
        setStatus(resolved.value("error").toString());
        return false;
    }
    return applyResolvedScene(scene, resolved);
}

bool Backend::applyResolvedScene(const QJsonObject &scene, const QVariantMap &resolved)
{
    const bool isSessionRestore = scene.isEmpty();
    const QVariantList matches = resolved.value("matches").toList();
    if (matches.isEmpty()) {
        setStatus(tr("No scene windows are currently open. Nothing was moved or launched."));
        return false;
    }
    for (const QVariant &value : matches) {
        const QVariantMap saved = value.toMap().value("saved").toMap();
        const QVariantMap rect = saved.value("rect").toMap();
        const double x = rect.value("x").toDouble();
        const double y = rect.value("y").toDouble();
        const double width = rect.value("width").toDouble();
        const double height = rect.value("height").toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
            !std::isfinite(height) || x < 0.0 || y < 0.0 || width <= 0.0 ||
            height <= 0.0 || width > 1.0 || height > 1.0 ||
            x + width > 1.001 || y + height > 1.001) {
            setStatus(tr("Scene geometry is invalid. No windows were changed."));
            return false;
        }
        const QVariantMap entry = value.toMap();
        const QVariantMap monitor = entry.value("monitor", resolved.value("targetMonitor")).toMap();
        if (monitor.value("width").toInt() <= 0 || monitor.value("height").toInt() <= 0) {
            setStatus(tr("A scene display’s geometry is unavailable. No windows were changed."));
            return false;
        }
    }

    int applied = 0;
    for (const QVariant &value : matches) {
        const QVariantMap entry = value.toMap();
        const QVariantMap window = entry.value("window").toMap();
        const QVariantMap saved = entry.value("saved").toMap();
        const QVariantMap rect = saved.value("rect").toMap();
        const QVariantMap monitor = entry.value("monitor", resolved.value("targetMonitor")).toMap();
        const int monitorX = monitor.value("x", 0).toInt();
        const int monitorY = monitor.value("y", 0).toInt();
        const int monitorWidth = monitor.value("width").toInt();
        const int monitorHeight = monitor.value("height").toInt();
        const double widthRatio = rect.value("width").toDouble();
        const double heightRatio = rect.value("height").toDouble();
        const QVariantMap settings{
            {"monitor", entry.value("monitorDescription", resolved.value("monitor")).toString()},
            {"workspace", entry.value("workspace", resolved.value("workspace")).toString()},
            {"layout", saved.value("floating").toBool() ? "floating" : "tiled"},
            {"width", qMax(1, qRound(widthRatio * monitorWidth))},
            {"height", qMax(1, qRound(heightRatio * monitorHeight))},
            {"center", false},
            {"setPosition", true},
            {"x", monitorX + qRound(rect.value("x").toDouble() * monitorWidth)},
            {"y", monitorY + qRound(rect.value("y").toDouble() * monitorHeight)}
        };
        const QString code = liveApplyCode(window, settings);
        if (code.isEmpty()) continue;
        QProcess process;
        process.start(m_hyprctl, {"eval", code});
        const bool finished = process.waitForStarted(1500) && process.waitForFinished(4000) &&
            process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        const QByteArray response = process.readAllStandardOutput().trimmed();
        if (!finished || (!response.isEmpty() && response != "ok")) {
            setStatus(isSessionRestore
                ? tr("Session partially restored to %1 of %2 window(s). Remaining windows were left unchanged.")
                    .arg(applied).arg(matches.size())
                : tr("Scene partially applied to %1 of %2 window(s). Remaining windows were left unchanged.")
                    .arg(applied).arg(matches.size()));
            return false;
        }
        ++applied;
    }
    const int missing = resolved.value("missing").toInt();
    setStatus(isSessionRestore
        ? (missing > 0
            ? tr("Session restored to %1 open window(s). %2 missing window(s) were skipped; no apps were launched.")
                .arg(applied).arg(missing)
            : tr("Session restored to %1 open window(s).").arg(applied))
        : (missing > 0
            ? tr("Scene applied to %1 open window(s). %2 missing app window(s) were not launched.")
                .arg(applied).arg(missing)
            : tr("Scene applied to %1 open window(s).").arg(applied)));
    return applied > 0;
}

bool Backend::removeScene(const QString &id)
{
    QJsonArray scenes = m_library.value("scenes").toArray();
    QJsonArray retained;
    bool removed = false;
    for (const QJsonValue &value : scenes) {
        if (value.toObject().value("id").toString() == id) removed = true;
        else retained.append(value);
    }
    if (!removed) { setStatus(tr("Scene not found.")); return false; }
    QJsonObject library = m_library;
    library.insert("scenes", retained);
    if (!writeLibrary(library)) return false;
    setStatus(tr("Scene removed."));
    return true;
}

QVariantList Backend::sessionSnapshots() const
{
    QVariantList result;
    for (const QJsonValue &value : m_library.value("sessionSnapshots").toArray()) {
        const QJsonObject snapshot = value.toObject();
        result.append(QVariantMap{{"id", snapshot.value("id").toString()},
            {"createdAt", snapshot.value("createdAt").toString()},
            {"windowCount", snapshot.value("windowCount").toInt()}});
    }
    return result;
}

bool Backend::sessionSnapshotsEnabled() const
{
    return m_library.value("preferences").toObject()
        .value("sessionSnapshotsEnabled").toBool(false);
}

bool Backend::setSessionSnapshotsEnabled(bool enabled)
{
    QJsonObject preferences = m_library.value("preferences").toObject();
    if (preferences.value("sessionSnapshotsEnabled").toBool(false) == enabled) return true;
    preferences.insert("sessionSnapshotsEnabled", enabled);
    QJsonObject library = m_library;
    library.insert("preferences", preferences);
    if (!writeLibrary(library)) return false;
    if (enabled) {
        refreshWindows();
        saveSessionSnapshot();
    } else {
        m_sessionSnapshotTimer.stop();
    }
    setStatus(enabled
        ? tr("Event-driven session snapshots enabled. Existing windows were captured; restore remains manual.")
        : tr("Event-driven session snapshots disabled. Existing snapshots are kept."));
    return true;
}

void Backend::saveSessionSnapshot()
{
    if (!sessionSnapshotsEnabled() || m_windows.isEmpty()) return;
    QHash<QString, QJsonObject> monitors;
    for (const QVariant &value : m_monitors) {
        const QJsonObject monitor = QJsonObject::fromVariantMap(value.toMap());
        const QString description = monitor.value("description").toString();
        if (!description.isEmpty()) monitors.insert(QString::number(monitor.value("id").toInt()), monitor);
    }

    QHash<QString, QJsonArray> grouped;
    QHash<QString, QJsonObject> groupInfo;
    int savedCount = 0;
    for (const QVariant &value : m_windows) {
        const QVariantMap window = value.toMap();
        const QString monitorId = QString::number(window.value("monitor").toInt());
        const QJsonObject monitor = monitors.value(monitorId);
        const QString klass = window.value("class").toString().trimmed();
        const QString workspace = window.value("workspace").toString().trimmed();
        const auto at = window.value("at").toList();
        const auto size = window.value("size").toList();
        const int mw = monitor.value("width").toInt(), mh = monitor.value("height").toInt();
        if (monitor.isEmpty() || klass.isEmpty() || workspace.isEmpty() ||
            workspace.startsWith("special:") || window.value("fullscreen").toInt() != 0 ||
            at.size() != 2 || size.size() != 2 || mw <= 0 || mh <= 0 ||
            size[0].toInt() <= 0 || size[1].toInt() <= 0) continue;
        const QString key = monitor.value("description").toString() + "\n" + workspace;
        const double width = qBound(0.05, size[0].toDouble() / mw, 1.0);
        const double height = qBound(0.05, size[1].toDouble() / mh, 1.0);
        const double x = qBound(0.0, (at[0].toDouble() - monitor.value("x").toInt()) / mw, 1.0 - width);
        const double y = qBound(0.0, (at[1].toDouble() - monitor.value("y").toInt()) / mh, 1.0 - height);
        grouped[key].append(QJsonObject{{"class", klass}, {"floating", window.value("floating").toBool()},
            {"rect", QJsonObject{{"x", x}, {"y", y}, {"width", width}, {"height", height}}}});
        groupInfo.insert(key, QJsonObject{{"monitor", monitor.value("description").toString()},
            {"workspace", workspace}});
        ++savedCount;
    }
    if (savedCount == 0) return;
    QJsonArray groups;
    QStringList groupKeys = grouped.keys();
    std::sort(groupKeys.begin(), groupKeys.end());
    for (const QString &key : groupKeys) {
        QJsonObject group = groupInfo.value(key);
        QJsonArray windows = grouped.value(key);
        QList<QJsonObject> orderedWindows;
        for (const QJsonValue &window : windows) orderedWindows.append(window.toObject());
        std::sort(orderedWindows.begin(), orderedWindows.end(), [](const QJsonObject &a, const QJsonObject &b) {
            const double ax = a.value("rect").toObject().value("x").toDouble();
            const double bx = b.value("rect").toObject().value("x").toDouble();
            if (ax != bx) return ax < bx;
            return a.value("rect").toObject().value("y").toDouble() < b.value("rect").toObject().value("y").toDouble();
        });
        windows = QJsonArray();
        for (const QJsonObject &window : orderedWindows) windows.append(window);
        group.insert("windows", windows);
        groups.append(group);
    }
    QJsonArray snapshots = m_library.value("sessionSnapshots").toArray();
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString stateFingerprint = QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument(groups).toJson(QJsonDocument::Compact),
                                 QCryptographicHash::Sha256).toHex());
    if (!snapshots.isEmpty() &&
        snapshots.first().toObject().value("stateFingerprint").toString() == stateFingerprint) {
        return;
    }
    QJsonArray ordered;
    ordered.append(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"createdAt", now}, {"windowCount", savedCount}, {"stateFingerprint", stateFingerprint},
        {"groups", groups}});
    for (const QJsonValue &snapshot : snapshots)
        if (ordered.size() < 10) ordered.append(snapshot);
    QJsonObject library = m_library;
    library.insert("sessionSnapshots", ordered);
    if (writeLibrary(library)) setStatus(tr("Session snapshot saved (%1 window(s)).").arg(savedCount));
}

QVariantMap Backend::resolveSessionSnapshot(const QJsonObject &snapshot) const
{
    QVariantList matches, items;
    QSet<QString> used;
    int missing = 0;
    const QJsonArray groups = snapshot.value("groups").toArray();
    const int recordedCount = snapshot.value("windowCount").toInt();
    if (groups.isEmpty() || groups.size() > 32 || recordedCount <= 0 || recordedCount > 128)
        return {{"error", tr("This snapshot has an invalid window or display count.")}};
    int validatedCount = 0;
    for (const QJsonValue &groupValue : groups) {
        const QJsonObject group = groupValue.toObject();
        const QString monitorName = group.value("monitor").toString().trimmed();
        const QString workspace = group.value("workspace").toString().trimmed();
        const QJsonArray savedWindows = group.value("windows").toArray();
        if (monitorName.isEmpty() || workspace.isEmpty() || workspace.startsWith("special:") ||
            savedWindows.isEmpty() || savedWindows.size() > 128)
            return {{"error", tr("This snapshot contains an invalid display or workspace.")}};
        validatedCount += savedWindows.size();
        if (validatedCount > 128)
            return {{"error", tr("This snapshot exceeds the safe window limit.")}};
        for (const QJsonValue &windowValue : savedWindows) {
            const QJsonObject saved = windowValue.toObject();
            const QJsonObject rect = saved.value("rect").toObject();
            const double x = rect.value("x").toDouble(-1), y = rect.value("y").toDouble(-1);
            const double width = rect.value("width").toDouble(-1), height = rect.value("height").toDouble(-1);
            if (saved.value("class").toString().trimmed().isEmpty() ||
                !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
                !std::isfinite(height) || x < 0 || y < 0 || width <= 0 || height <= 0 ||
                width > 1 || height > 1 || x + width > 1.001 || y + height > 1.001)
                return {{"error", tr("This snapshot contains invalid window geometry.")}};
        }
    }
    if (validatedCount != recordedCount)
        return {{"error", tr("This snapshot’s saved window count does not match its data.")}};
    for (const QJsonValue &groupValue : groups) {
        const QJsonObject group = groupValue.toObject();
        QVariantMap monitor;
        for (const QVariant &value : m_monitors) {
            const QVariantMap candidate = value.toMap();
            if (candidate.value("description").toString() == group.value("monitor").toString()) {
                monitor = candidate;
                break;
            }
        }
        if (monitor.isEmpty())
            return {{"error", tr("A display from this snapshot is disconnected. No windows were changed.")}};
        QVariantList candidates;
        for (const QVariant &value : m_windows) {
            const QVariantMap window = value.toMap();
            if (!window.value("address").toString().isEmpty() &&
                !window.value("class").toString().isEmpty() &&
                window.value("fullscreen").toInt() == 0 &&
                !window.value("workspace").toString().startsWith("special:"))
                candidates.append(window);
        }
        std::sort(candidates.begin(), candidates.end(), [](const QVariant &a, const QVariant &b) {
            const auto aa = a.toMap().value("at").toList(), bb = b.toMap().value("at").toList();
            if (aa.size() < 2 || bb.size() < 2) return a.toMap().value("address").toString() < b.toMap().value("address").toString();
            return aa[0].toInt() == bb[0].toInt() ? aa[1].toInt() < bb[1].toInt() : aa[0].toInt() < bb[0].toInt();
        });
        for (const QJsonValue &savedValue : group.value("windows").toArray()) {
            const QJsonObject saved = savedValue.toObject();
            QVariantMap match;
            for (const QVariant &candidateValue : candidates) {
                const QVariantMap candidate = candidateValue.toMap();
                const QString address = candidate.value("address").toString();
                if (!used.contains(address) && candidate.value("class").toString().compare(
                        saved.value("class").toString(), Qt::CaseInsensitive) == 0) {
                    match = candidate;
                    used.insert(address);
                    break;
                }
            }
            const QString klass = saved.value("class").toString();
            if (match.isEmpty()) {
                ++missing;
                items.append(QVariantMap{{"class", klass}, {"matched", false}});
            } else {
                matches.append(QVariantMap{{"saved", saved.toVariantMap()}, {"window", match},
                    {"monitor", monitor}, {"monitorDescription", group.value("monitor").toString()},
                    {"workspace", group.value("workspace").toString()}});
                items.append(QVariantMap{{"class", klass}, {"matched", true}});
            }
        }
    }
    return {{"matches", matches}, {"items", items}, {"matched", matches.size()},
        {"missing", missing}};
}

QVariantMap Backend::previewSessionSnapshot(const QString &id) const
{
    for (const QJsonValue &value : m_library.value("sessionSnapshots").toArray()) {
        const QJsonObject snapshot = value.toObject();
        if (snapshot.value("id").toString() != id) continue;
        QVariantMap result = resolveSessionSnapshot(snapshot);
        result.insert("id", id);
        result.insert("createdAt", snapshot.value("createdAt").toString());
        result.insert("previewOnly", true);
        result.remove("matches");
        return result;
    }
    return {{"error", tr("Session snapshot not found.")}, {"matched", 0}, {"missing", 0}};
}

bool Backend::restoreSessionSnapshot(const QString &id)
{
    if (trialActive() || m_monitorRevertPending) {
        setStatus(tr("Finish or revert the current display/geometry trial before restoring a session."));
        return false;
    }
    for (const QJsonValue &value : m_library.value("sessionSnapshots").toArray()) {
        const QJsonObject snapshot = value.toObject();
        if (snapshot.value("id").toString() != id) continue;
        const QVariantMap resolved = resolveSessionSnapshot(snapshot);
        if (!resolved.value("error").toString().isEmpty()) {
            setStatus(resolved.value("error").toString());
            return false;
        }
        QJsonObject envelope;
        return applyResolvedScene(envelope, resolved);
    }
    setStatus(tr("Session snapshot not found."));
    return false;
}

QVariantList Backend::presets() const
{
    QVariantList result;
    for (const auto &entry : m_library.value("presets").toArray()) {
        const auto item = entry.toObject();
        result.append(QVariantMap{{"id", item.value("id").toString()}, {"name", item.value("name").toString()}});
    }
    return result;
}

QVariantList Backend::profiles() const
{
    QVariantList result;
    for (const auto &entry : m_library.value("profiles").toArray()) {
        const auto item = entry.toObject();
        result.append(QVariantMap{{"id", item.value("id").toString()}, {"name", item.value("name").toString()},
                                  {"count", item.value("rules").toArray().size()}});
    }
    return result;
}

bool Backend::savePreset(const QString &name, const QVariantMap &settings)
{
    if (name.trimmed().isEmpty() || !validateSettings(settings).isEmpty()) {
        setStatus(tr("Preset needs a name and valid settings.")); return false;
    }
    auto items = m_library.value("presets").toArray();
    for (const auto &item : items)
        if (item.toObject().value("name").toString() == name.trimmed()) {
            setStatus(tr("That preset name already exists. Choose another name.")); return false;
        }
    QVariantMap s = settings;
    // Presets are effects, not targets. Applying one must not silently broaden
    // an application's matching scope or copy a personal window title.
    for (const auto &key : {"match", "matchTitle", "classMatchMode", "titleMatchMode", "excludeTitle"})
        s.remove(key);
    items.append(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"name", name.trimmed().left(120)}, {"settings", QJsonObject::fromVariantMap(s)}});
    auto library = m_library; library.insert("presets", items);
    if (!writeLibrary(library)) return false;
    setStatus(tr("Custom preset saved. Matching scope was not included."));
    return true;
}

QVariantMap Backend::presetSettings(const QString &id) const
{
    for (const auto &entry : m_library.value("presets").toArray())
        if (entry.toObject().value("id").toString() == id) return entry.toObject().value("settings").toObject().toVariantMap();
    return {};
}

bool Backend::saveProfile(const QString &name)
{
    if (name.trimmed().isEmpty()) { setStatus(tr("Give the profile a name.")); return false; }
    auto profiles = m_library.value("profiles").toArray();
    for (const auto &entry : profiles)
        if (entry.toObject().value("name").toString() == name.trimmed()) {
            setStatus(tr("That profile name already exists.")); return false;
        }
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }
    profiles.append(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"name", name.trimmed().left(120)}, {"rules", rules}});
    auto library = m_library; library.insert("profiles", profiles);
    if (!writeLibrary(library)) return false;
    setStatus(tr("Profile saved. Switching is manual; no automatic desktop changes are scheduled."));
    return true;
}

QString Backend::profilePreview(const QString &id) const
{
    for (const auto &entry : m_library.value("profiles").toArray())
        if (entry.toObject().value("id").toString() == id)
            return QString::fromUtf8(QJsonDocument(entry.toObject().value("rules").toArray()).toJson());
    return {};
}

bool Backend::activateProfile(const QString &id)
{
    const auto preview = profilePreview(id);
    if (preview.isEmpty()) { setStatus(tr("Profile not found.")); return false; }
    if (!importRules(preview)) return false;
    setStatus(tr("Profile activated and validated. Undo is available; reopen windows for creation-time effects."));
    return true;
}

bool Backend::removeLibraryItem(const QString &kind, const QString &id)
{
    if (kind != "presets" && kind != "profiles") return false;
    auto items = m_library.value(kind).toArray();
    bool found = false;
    for (int i = items.size() - 1; i >= 0; --i)
        if (items[i].toObject().value("id").toString() == id) { items.removeAt(i); found = true; }
    if (!found) return false;
    auto library = m_library; library.insert(kind, items);
    if (!writeLibrary(library)) return false;
    setStatus(tr("Library item removed."));
    return true;
}

QVariant Backend::preference(const QString &key, const QVariant &fallback) const
{
    const auto prefs = m_library.value("preferences").toObject();
    return prefs.contains(key) ? prefs.value(key).toVariant() : fallback;
}

bool Backend::setPreference(const QString &key, const QVariant &value)
{
    if (key != "onboarded" && key != "advanced" && key != "textScale") return false;
    if (key == "textScale" && (value.toDouble() < 0.85 || value.toDouble() > 1.4)) return false;
    auto prefs = m_library.value("preferences").toObject();
    prefs.insert(key, QJsonValue::fromVariant(value));
    auto library = m_library; library.insert("preferences", prefs);
    return writeLibrary(library);
}
