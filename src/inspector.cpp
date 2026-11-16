#include "backend.h"
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>

namespace {
QVariantMap effects(const QVariantMap &s)
{
    QVariantMap result;
    const QString layout = s.value("layout", "floating").toString();
    if (layout != "keep") result.insert("layout", layout);
    if (layout == "floating") {
        result.insert("width", s.value("width", 600));
        result.insert("height", s.value("height", 420));
        if (s.value("center", true).toBool()) result.insert("position", "center");
        else if (s.value("setPosition").toBool())
            result.insert("position", QString("%1,%2").arg(s.value("x").toInt()).arg(s.value("y").toInt()));
    }
    for (const auto &key : {"pin", "noBlur", "noShadow", "noInitialFocus", "noAnim", "noDim", "noVrr", "persistentSize"})
        if (s.value(key).toBool()) result.insert(key, true);
    for (const auto &key : {"workspace", "monitor"})
        if (!s.value(key).toString().isEmpty()) result.insert(key, s.value(key));
    if (s.value("opacityEnabled").toBool()) {
        result.insert("opacityActive", s.value("opacityActive", 1));
        result.insert("opacityInactive", s.value("opacityInactive", 0.9));
    }
    if (s.value("borderEnabled").toBool()) result.insert("borderSize", s.value("borderSize", 2));
    if (s.value("roundingEnabled").toBool()) result.insert("rounding", s.value("rounding", 8));
    return result;
}
}

QVariantMap Backend::matchWindow(const QVariantMap &window, const QVariantMap &settings)
{
    QVariantMap match = settings.value("match").toMap();
    if (match.isEmpty()) return {{"matched", false}, {"unknown", true}, {"reason", tr("No explicit match supplied.")}};
    bool unknown = false;
    QStringList reasons;
    const QMap<QString, QString> aliases{{"class", "currentClass"}, {"title", "currentTitle"},
        {"initial_class", "class"}, {"initial_title", "title"}, {"xdg_tag", "xdgTag"},
        {"group", "grouped"}, {"float", "floating"}, {"pin", "pinned"}, {"content", "contentType"}};
    for (auto it = match.cbegin(); it != match.cend(); ++it) {
        QString field = aliases.value(it.key(), it.key());
        if (it.key() == "class" && !window.contains(field)) field = "class";
        if (it.key() == "title" && !window.contains(field)) field = "title";
        if (!window.contains(field)) { unknown = true; reasons << tr("Cannot evaluate %1 from client data.").arg(it.key()); continue; }
        bool matches = false;
        if (it.key() == "class" || it.key() == "title" || it.key() == "initial_class" || it.key() == "initial_title") {
            const QString mode = settings.value(it.key().contains("class") ? "classMatchMode" : "titleMatchMode", "exact").toString();
            const QString expected = it.value().toString(), actual = window.value(field).toString();
            if (mode == "exact") matches = actual == expected;
            else if (mode == "contains") matches = actual.contains(expected);
            else {
                const QRegularExpression regex(QRegularExpression::anchoredPattern(expected));
                if (!regex.isValid()) return {{"matched", false}, {"unknown", true}, {"reason", regex.errorString()}};
                matches = regex.match(actual).hasMatch();
            }
        } else if (it.value().metaType().id() == QMetaType::Bool)
            matches = window.value(field).toBool() == it.value().toBool();
        else matches = window.value(field).toString() == it.value().toString();
        if (!matches) return {{"matched", false}, {"unknown", false}, {"reason", tr("%1 does not match.").arg(it.key())}};
    }
    const QString exclude = settings.value("excludeTitle").toString().trimmed();
    if (!exclude.isEmpty() && !match.contains("title")) {
        QRegularExpression regex(QRegularExpression::anchoredPattern(exclude));
        if (!regex.isValid()) return {{"matched", false}, {"unknown", true}, {"reason", regex.errorString()}};
        if (regex.match(window.value("currentTitle", window.value("title")).toString()).hasMatch())
            return {{"matched", false}, {"unknown", false}, {"reason", tr("Title is excluded.")}};
    }
    return {{"matched", !unknown}, {"unknown", unknown}, {"reason", unknown ? reasons.join("\n") : tr("All available conditions match.")}};
}

QVariantMap Backend::testMatch(const QVariantMap &settings) const
{
    QVariantList matches, uncertain;
    for (const auto &entry : m_windows) {
        const auto row = entry.toMap();
        auto verdict = matchWindow(row, settings);
        verdict.insert("window", row);
        if (verdict.value("matched").toBool()) matches.append(verdict);
        else if (verdict.value("unknown").toBool()) uncertain.append(verdict);
    }
    return {{"matches", matches}, {"uncertain", uncertain}, {"count", matches.size()},
        {"coverage", tr("Local approximation using current client data. Regex uses Qt/PCRE, which can differ from Hyprland. Unknown conditions are never treated as matches.")}};
}

QVariantMap Backend::inspectWindow() const
{
    const auto window = selectedWindow();
    QVariantList matching;
    QStringList conflicts;
    QMap<QString, QString> requested;
    for (int i = 0; i < m_savedRules.size(); ++i) {
        const auto rule = m_savedRules[i].toMap();
        if (!rule.value("enabled", true).toBool()) continue;
        auto settings = resolvePlacement(rule.value("settings").toMap());
        settings.insert("match", rule.value("match"));
        const auto verdict = matchWindow(window, settings);
        if (!verdict.value("matched").toBool() && !verdict.value("unknown").toBool()) continue;
        const auto requestedEffects = effects(settings);
        matching.append(QVariantMap{{"index", i}, {"name", rule.value("name", rule.value("class"))},
            {"effects", requestedEffects}, {"summary", summarize(settings)}, {"unknown", verdict.value("unknown")},
            {"reason", verdict.value("reason")}});
        if (verdict.value("unknown").toBool()) continue;
        for (auto it = requestedEffects.cbegin(); it != requestedEffects.cend(); ++it) {
            const QString value = it.value().toString();
            if (requested.contains(it.key()) && requested.value(it.key()) != value)
                conflicts << tr("%1: matching rules request both %2 and %3.").arg(it.key(), requested.value(it.key()), value);
            requested.insert(it.key(), value);
        }
    }
    return {{"window", window}, {"rules", matching}, {"conflicts", conflicts}, {"externalHints", externalRuleHints()},
        {"coverage", tr("Shows requested values, not guaranteed effective precedence. Only WindowTuner rules are evaluated. Main-config hints do not follow includes or execute Lua. Some changes require reopening windows.")}};
}

QVariantList Backend::externalRuleHints() const
{
    QFile config(m_hyprlandConfig);
    QVariantList result;
    if (!config.open(QIODevice::ReadOnly)) return result;
    int line = 0;
    while (!config.atEnd() && line < 20000) {
        const QString text = QString::fromUtf8(config.readLine(65536)).trimmed();
        ++line;
        if (text.startsWith("--")) continue;
        if (text.contains("o.window") || text.contains("hl.window_rule") || text.contains("windowrule") || (text.contains("require(") && !text.contains("hypr.windowtuner")))
            result.append(QVariantMap{{"line", line}, {"kind", text.contains("require(") ? tr("Module include (not followed)") : tr("Possible external window rule")}});
    }
    return result;
}
