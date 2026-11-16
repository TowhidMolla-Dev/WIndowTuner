#include "backend.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QSet>
#include <algorithm>

QVariantList Backend::installedApplications() const
{
    QVariantList result;
    QSet<QString> seen;
    for (const QString &root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        QDirIterator it(root, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && result.size() < 2000) {
            const QString path = it.next();
            const QString id = QDir(root).relativeFilePath(path);
            if (seen.contains(id)) continue;
            seen.insert(id);
            QSettings entry(path, QSettings::IniFormat);
            entry.beginGroup("Desktop Entry");
            if (entry.value("Hidden", false).toBool() || entry.value("NoDisplay", false).toBool() ||
                entry.value("Type", "Application").toString() != "Application") continue;
            const QString declared = entry.value("StartupWMClass").toString();
            result.append(QVariantMap{{"name", entry.value("Name", QFileInfo(path).baseName()).toString()},
                {"class", declared.isEmpty() ? QFileInfo(path).baseName() : declared},
                {"inferred", declared.isEmpty()}});
        }
    }
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("name").toString().localeAwareCompare(b.toMap().value("name").toString()) < 0;
    });
    return result;
}

bool Backend::selectOfflineTarget(const QString &klass, const QString &title)
{
    if (klass.trimmed().isEmpty()) { setStatus(tr("Enter the application's window class.")); return false; }
    m_offlineTarget = {{"class", klass.trimmed()}, {"title", title}, {"currentTitle", title},
        {"offline", true}, {"workspace", ""}, {"xdgTag", ""}, {"contentType", ""},
        {"size", QVariantList{0, 0}}, {"at", QVariantList{0, 0}}};
    m_selectedIndex = -1;
    emit selectedChanged();
    setStatus(tr("Offline target selected. Desktop-entry class hints may differ from the real Wayland app ID; test after opening the app."));
    return true;
}

bool Backend::setRuleMetadata(const QString &id, const QString &name, const QString &folder, const QString &tags)
{
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }
    if (name.trimmed().isEmpty()) { setStatus(tr("Rule name cannot be empty.")); return false; }
    for (int i = 0; i < rules.size(); ++i) {
        auto rule = rules[i].toObject();
        if (rule.value("id").toString() != id) continue;
        rule.insert("name", name.trimmed().left(120));
        rule.insert("folder", folder.trimmed().left(120));
        QStringList cleaned;
        for (const auto &tag : tags.split(',', Qt::SkipEmptyParts))
            if (!tag.trimmed().isEmpty()) cleaned << tag.trimmed().left(60);
        cleaned.removeDuplicates();
        rule.insert("tags", QJsonArray::fromStringList(cleaned));
        rules.replace(i, rule);
        if (!commitRuleData(rules)) return false;
        setStatus(tr("Rule name, folder and tags saved."));
        return true;
    }
    setStatus(tr("Rule no longer exists."));
    return false;
}

bool Backend::batchSetEnabled(const QVariantList &ids, bool enabled)
{
    if (ids.isEmpty()) { setStatus(tr("Select at least one rule.")); return false; }
    QJsonArray rules; QString error;
    if (!readRuleData(&rules, &error)) { setStatus(error); return false; }
    QSet<QString> requested;
    for (const auto &id : ids) requested.insert(id.toString());
    for (int i = 0; i < rules.size(); ++i) {
        auto rule = rules[i].toObject();
        if (!requested.remove(rule.value("id").toString())) continue;
        rule.insert("enabled", enabled); rules.replace(i, rule);
    }
    if (!requested.isEmpty()) { setStatus(tr("Selection is stale. Refresh and select the rules again.")); return false; }
    if (!commitRuleData(rules)) return false;
    setStatus(tr("Selected rules updated in one transaction."));
    return true;
}

QString Backend::exportSelected(const QVariantList &ids) const
{
    QJsonArray rules, selected; QString error;
    if (!readRuleData(&rules, &error)) return {};
    for (const auto &entry : rules)
        if (ids.contains(entry.toObject().value("id").toString())) selected.append(entry);
    return QString::fromUtf8(QJsonDocument(selected).toJson());
}

bool Backend::decodeRules(const QString &json, QJsonArray *rules, QString *error) const
{
    const auto fail = [&](const QString &why) { if (error) *error = tr("Import rejected: ") + why; return false; };
    if (json.toUtf8().size() > 4 * 1024 * 1024) return fail(tr("maximum import size is 4 MiB."));
    const auto document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray() || document.array().size() > 1000) return fail(tr("expected an array of at most 1000 rules."));
    QSet<QString> ids;
    for (const auto &entry : document.array()) {
        if (!entry.isObject()) return fail(tr("each rule must be an object."));
        auto rule = entry.toObject();
        if (rule.value("class").toString().trimmed().isEmpty() || !rule.value("settings").isObject())
            return fail(tr("every rule needs a class and settings object."));
        auto match = rule.value("match").toObject();
        if (match.isEmpty()) {
            match.insert("class", rule.value("class"));
            if (!rule.value("title").toString().isEmpty()) match.insert("title", rule.value("title"));
        }
        if (match.value("class").toString().isEmpty() && match.value("initial_class").toString().isEmpty())
            return fail(tr("every rule must include a class match."));
        auto settings = resolvePlacement(rule.value("settings").toObject().toVariantMap());
        settings.insert("match", match.toVariantMap());
        const auto problems = validateSettings(settings);
        if (!problems.isEmpty()) return fail(problems.join("\n"));
        rule.insert("match", match);
        rule.insert("settings", QJsonObject::fromVariantMap(settings));
        auto id = rule.value("id").toString();
        if (id.isEmpty() || ids.contains(id)) id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ids.insert(id); rule.insert("id", id);
        if (rule.value("name").toString().isEmpty()) rule.insert("name", rule.value("class"));
        if (!rule.contains("enabled")) rule.insert("enabled", true);
        rules->append(rule);
    }
    return true;
}

QVariantMap Backend::previewImport(const QString &json) const
{
    QJsonArray rules; QString error;
    if (!decodeRules(json, &rules, &error)) return {{"error", error}, {"rules", QVariantList{}}};
    QVariantList rows;
    for (int i = 0; i < rules.size(); ++i) {
        auto rule = rules[i].toObject().toVariantMap(); rule.insert("index", i); rows.append(rule);
    }
    return {{"rules", rows}, {"count", rules.size()}};
}

bool Backend::importSelection(const QString &json, const QVariantList &indexes, const QString &mode)
{
    if (mode != "merge" && mode != "replace") { setStatus(tr("Choose Merge or Replace.")); return false; }
    QJsonArray incoming, result; QString error;
    if (!decodeRules(json, &incoming, &error)) { setStatus(error); return false; }
    if (indexes.isEmpty()) { setStatus(tr("Choose at least one incoming rule.")); return false; }
    if (mode == "merge" && !readRuleData(&result, &error)) { setStatus(error); return false; }
    QSet<QString> ids;
    for (const auto &entry : result) ids.insert(entry.toObject().value("id").toString());
    QSet<int> seen;
    for (const auto &value : indexes) {
        bool ok;
        const int index = value.toInt(&ok);
        if (!ok || index < 0 || index >= incoming.size()) { setStatus(tr("Invalid import selection.")); return false; }
        if (seen.contains(index)) continue;
        seen.insert(index);
        auto rule = incoming[index].toObject();
        if (ids.contains(rule.value("id").toString())) rule.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
        ids.insert(rule.value("id").toString());
        result.append(rule);
    }
    if (!commitRuleData(result, true)) return false;
    setStatus(tr("Selected rules imported. Merge preserves existing rules; colliding IDs become new copies."));
    return true;
}
