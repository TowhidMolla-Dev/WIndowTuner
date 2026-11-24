#include "backend.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <memory>

namespace {
constexpr int maxPayload = 4 * 1024 * 1024;
constexpr int saltLength = 16, nonceLength = 12, tagLength = 16;
const QByteArray prefix("WTM1:");

struct Key {
    unsigned char bytes[32]{};
    ~Key() { OPENSSL_cleanse(bytes, sizeof bytes); }
};

bool derive(const QString &passphrase, const unsigned char *salt, Key &key)
{
    auto password = passphrase.toUtf8();
    const bool ok = EVP_PBE_scrypt(password.constData(), password.size(), salt, saltLength,
        32768, 8, 1, 64 * 1024 * 1024, key.bytes, sizeof key.bytes) == 1;
    OPENSSL_cleanse(password.data(), password.size());
    return ok;
}

QByteArray seal(const QByteArray &plain, const QString &passphrase)
{
    QByteArray envelope(saltLength + nonceLength + tagLength + plain.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    auto *salt = reinterpret_cast<unsigned char *>(envelope.data());
    auto *nonce = salt + saltLength, *tag = nonce + nonceLength, *cipher = tag + tagLength;
    Key key;
    if (RAND_bytes(salt, saltLength + nonceLength) != 1 || !derive(passphrase, salt, key)) return {};
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int length = 0, written = 0;
    if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.bytes, nonce) != 1 ||
        EVP_EncryptUpdate(ctx.get(), nullptr, &length, reinterpret_cast<const unsigned char *>(prefix.constData()), prefix.size()) != 1 ||
        EVP_EncryptUpdate(ctx.get(), cipher, &written, reinterpret_cast<const unsigned char *>(plain.constData()), plain.size()) != 1 ||
        EVP_EncryptFinal_ex(ctx.get(), cipher + written, &length) != 1 || written + length != plain.size() ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tagLength, tag) != 1)
        return {};
    envelope.resize(saltLength + nonceLength + tagLength + plain.size());
    return prefix + envelope.toBase64();
}

QByteArray unseal(const QByteArray &bundle, const QString &passphrase)
{
    if (!bundle.startsWith(prefix) || bundle.size() > 6 * 1024 * 1024) return {};
    auto decoded = QByteArray::fromBase64Encoding(bundle.mid(prefix.size()), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) return {};
    auto envelope = decoded.decoded;
    const int size = envelope.size() - saltLength - nonceLength - tagLength;
    if (size <= 0 || size > maxPayload) return {};
    auto *salt = reinterpret_cast<unsigned char *>(envelope.data());
    auto *nonce = salt + saltLength, *tag = nonce + nonceLength, *cipher = tag + tagLength;
    Key key;
    if (!derive(passphrase, salt, key)) return {};
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    QByteArray plain(size + EVP_MAX_BLOCK_LENGTH, '\0');
    auto *out = reinterpret_cast<unsigned char *>(plain.data());
    int length = 0, written = 0;
    if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.bytes, nonce) != 1 ||
        EVP_DecryptUpdate(ctx.get(), nullptr, &length, reinterpret_cast<const unsigned char *>(prefix.constData()), prefix.size()) != 1 ||
        EVP_DecryptUpdate(ctx.get(), out, &written, cipher, size) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tagLength, tag) != 1 ||
        EVP_DecryptFinal_ex(ctx.get(), out + written, &length) != 1 || written + length != size) {
        OPENSSL_cleanse(plain.data(), plain.size());
        return {};
    }
    plain.resize(size);
    return plain;
}

QJsonArray portableRules(const QJsonArray &rules)
{
    QJsonArray result;
    for (const auto &value : rules) {
        auto rule = value.toObject();
        auto settings = rule.value("settings").toObject();
        for (const auto &key : {"launchCommand", "launchDesktop", "autostart", "startupOnly", "startupWindow"})
            settings.remove(key);
        rule.insert("settings", settings);
        result.append(rule);
    }
    return result;
}

QJsonObject configuration(const QJsonArray &rules, const QJsonObject &library)
{
    return {{"rules", rules}, {"profiles", library.value("profiles").toArray()},
        {"scenes", library.value("scenes").toArray()}, {"workspaces", library.value("workspaces").toObject()}};
}

bool validPins(const QJsonObject &pins)
{
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        const auto slot = it.key().toInt();
        const auto pin = it.value().toObject();
        if (slot < 1 || slot > 10 || QString::number(slot) != it.key() ||
            !pin.value("monitor").isString() || !pin.value("persistent").isBool()) return false;
    }
    return true;
}

bool validScene(const QJsonObject &scene)
{
    const auto workspace = scene.value("workspace").toString();
    const auto windows = scene.value("windows").toArray();
    if (scene.value("monitor").toString().trimmed().isEmpty() || workspace.trimmed().isEmpty() ||
        workspace.startsWith("special:") || windows.isEmpty() || windows.size() > 32) return false;
    for (const auto &value : windows) {
        const auto window = value.toObject(), rect = window.value("rect").toObject();
        const auto x = rect.value("x").toDouble(-1), y = rect.value("y").toDouble(-1);
        const auto w = rect.value("width").toDouble(-1), h = rect.value("height").toDouble(-1);
        if (window.value("class").toString().trimmed().isEmpty() || !window.value("floating").isBool() ||
            x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > 1.001 || y + h > 1.001 ||
            window.contains("address") || window.contains("title")) return false;
    }
    return true;
}
}

QVariantMap Backend::exportMigration(const QString &passphrase) const
{
    if (passphrase.trimmed().size() < 12 || passphrase.size() > 1024)
        return {{"error", tr("Use a strong passphrase of 12–1024 characters.")}};
    QJsonArray rules;
    QString error;
    if (!readRuleData(&rules, &error)) return {{"error", error}};
    auto data = configuration(portableRules(rules), m_library);
    QJsonArray profiles;
    for (const auto &value : data.value("profiles").toArray()) {
        auto profile = value.toObject();
        profile.insert("rules", portableRules(profile.value("rules").toArray()));
        profiles.append(profile);
    }
    data.insert("profiles", profiles);
    const auto plain = QJsonDocument(data).toJson(QJsonDocument::Compact);
    if (plain.size() > maxPayload) return {{"error", tr("The portable settings exceed 4 MiB.")}};
    const auto bundle = seal(plain, passphrase);
    if (bundle.isEmpty()) return {{"error", tr("Could not encrypt the bundle.")}};
    return {{"bundle", QString::fromLatin1(bundle)}};
}

QVariantMap Backend::prepareMigration(const QString &bundle, const QString &passphrase, QJsonObject *prepared) const
{
    const auto fail = [](const QString &error) { return QVariantMap{{"error", error}}; };
    if (passphrase.isEmpty() || passphrase.size() > 1024 || bundle.size() > 6 * 1024 * 1024)
        return fail(tr("Enter a passphrase and a supported bundle under 6 MiB."));
    const auto plain = unseal(bundle.trimmed().toLatin1(), passphrase);
    if (plain.isEmpty()) return fail(tr("Could not unlock the bundle. The passphrase may be wrong or the bundle damaged."));
    auto incoming = QJsonDocument::fromJson(plain).object();
    if (!incoming.value("rules").isArray() || !incoming.value("profiles").isArray() ||
        !incoming.value("scenes").isArray() || !incoming.value("workspaces").isObject() ||
        !validPins(incoming.value("workspaces").toObject()))
        return fail(tr("This bundle contains an invalid settings structure."));
    QJsonArray normalized;
    QString error;
    const auto checkRules = [&](const QJsonArray &rules) {
        QJsonArray validated;
        // Keep portable values intact. Validation must not invent new IDs in the reviewed data.
        if (rules != portableRules(rules)) return false;
        QSet<QString> ids;
        for (const auto &rule : rules) {
            const auto id = rule.toObject().value("id").toString();
            if (id.isEmpty() || ids.contains(id)) return false;
            ids.insert(id);
        }
        return decodeRules(QString::fromUtf8(QJsonDocument(rules).toJson()), &validated, &error);
    };
    if (!checkRules(incoming.value("rules").toArray()))
        return fail(error.isEmpty() ? tr("The bundle contains nonportable rule settings.") : error);
    for (const auto &kind : {"profiles", "scenes"}) {
        QSet<QString> ids;
        const auto entries = incoming.value(kind).toArray();
        if (entries.size() > 256) return fail(tr("The bundle contains too many library entries."));
        for (const auto &value : entries) {
            const auto entry = value.toObject();
            const auto id = entry.value("id").toString();
            if (id.isEmpty() || ids.contains(id) || entry.value("name").toString().trimmed().isEmpty())
                return fail(tr("The bundle contains missing or duplicate library identifiers."));
            ids.insert(id);
            if (QString(kind) == "profiles") {
                if (!entry.value("rules").isArray() || !checkRules(entry.value("rules").toArray()) ||
                    (entry.contains("workspaces") && (!entry.value("workspaces").isObject() ||
                        !validPins(entry.value("workspaces").toObject()))))
                    return fail(tr("The bundle contains an invalid profile."));
            } else if (!validScene(entry)) {
                return fail(tr("The bundle contains an invalid scene."));
            }
        }
    }
    incoming = configuration(incoming.value("rules").toArray(), incoming);
    if (!readRuleData(&normalized, &error)) return fail(error);
    const auto before = configuration(normalized, m_library);

    QSet<QString> knownApps, displays, missingApps, missingDisplays;
    for (const auto &value : m_windows) {
        const auto app = value.toMap();
        knownApps.insert(app.value("class").toString());
        knownApps.insert(app.value("currentClass").toString());
    }
    for (const auto &value : installedApplications()) knownApps.insert(value.toMap().value("class").toString());
    for (const auto &value : m_monitors) {
        displays.insert(value.toMap().value("name").toString());
        displays.insert(value.toMap().value("description").toString());
    }
    const auto checkDisplay = [&](QString name) {
        if (name.startsWith("desc:")) name.remove(0, 5);
        if (!name.isEmpty() && !displays.contains(name)) missingDisplays.insert(name);
    };
    const auto checkClass = [&](const QString &name) {
        if (!name.isEmpty() && !knownApps.contains(name)) missingApps.insert(name);
    };
    const auto checkAssignments = [&](const QJsonObject &container) {
        const auto pins = container.value("workspaces").toObject();
        for (const auto &pin : pins) checkDisplay(pin.toObject().value("monitor").toString());
        for (const auto &value : container.value("rules").toArray()) {
            const auto rule = value.toObject(), settings = rule.value("settings").toObject();
            if (settings.value("classMatchMode").toString("exact") == "exact") {
                const auto match = rule.value("match").toObject();
                checkClass(match.value("class").toString(rule.value("class").toString()));
                checkClass(match.value("initial_class").toString());
            }
            checkDisplay(settings.value("monitor").toString());
            checkDisplay(settings.value("placement").toObject().value("monitor").toString());
        }
    };
    checkAssignments(incoming);
    for (const auto &profile : incoming.value("profiles").toArray()) checkAssignments(profile.toObject());
    for (const auto &value : incoming.value("scenes").toArray()) {
        const auto scene = value.toObject();
        checkDisplay(scene.value("monitor").toString());
        for (const auto &window : scene.value("windows").toArray()) checkClass(window.toObject().value("class").toString());
    }
    QStringList apps(missingApps.values()), monitors(missingDisplays.values());
    apps.sort();
    monitors.sort();
    const auto tokenData = QJsonDocument(QJsonArray{before, incoming, QString::fromLatin1(fingerprint().toHex()),
        QJsonArray::fromStringList(apps), QJsonArray::fromStringList(monitors)}).toJson(QJsonDocument::Compact);
    if (prepared) *prepared = incoming;
    return {{"before", QString::fromUtf8(QJsonDocument(before).toJson())},
        {"after", QString::fromUtf8(QJsonDocument(incoming).toJson())},
        {"missingApps", apps}, {"missingDisplays", monitors}, {"changed", before != incoming},
        {"token", QString::fromLatin1(QCryptographicHash::hash(tokenData, QCryptographicHash::Sha256).toHex())}};
}

QVariantMap Backend::previewMigration(const QString &bundle, const QString &passphrase) const
{
    return prepareMigration(bundle, passphrase, nullptr);
}

bool Backend::applyMigration(const QString &bundle, const QString &passphrase, const QString &token)
{
    if (trialActive() || monitorRevertPending()) {
        setStatus(tr("Finish the active trial before applying a settings bundle."));
        return false;
    }
    QJsonObject incoming;
    const auto review = prepareMigration(bundle, passphrase, &incoming);
    if (token.isEmpty() || review.value("token").toString() != token) {
        setStatus(review.value("error", tr("Settings or compatibility warnings changed. Preview the bundle again.")).toString());
        return false;
    }
    if (!review.value("changed").toBool()) return true;
    auto library = m_library;
    for (const auto &key : {"profiles", "scenes", "workspaces"}) library.insert(key, incoming.value(key));
    if (!commitRuleData(incoming.value("rules").toArray(), true, true, nullptr, nullptr, &library)) return false;
    setStatus(tr("Portable settings imported. No apps were launched or scenes restored."));
    return true;
}
