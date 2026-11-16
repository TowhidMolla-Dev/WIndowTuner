#include "backend.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>

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
