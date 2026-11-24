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
        if (!window.contains("currentTitle") && !window.contains("title"))
            return {{"matched", false}, {"unknown", true}, {"reason", tr("No window title is available to check the exclusion.")}};
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

QVariantMap Backend::simulateRule(const QVariantMap &settings) const
{
    QVariantList matches, unaffected, uncertain;
    for (const auto &entry : m_windows) {
        const auto window = entry.toMap();
        auto verdict = matchWindow(window, settings);
        verdict.insert("window", window);
        if (verdict.value("matched").toBool()) matches.append(verdict);
        else if (verdict.value("unknown").toBool()) uncertain.append(verdict);
        else unaffected.append(verdict);
    }
    return {{"matches", matches}, {"unaffected", unaffected}, {"uncertain", uncertain},
        {"coverage", tr("Local approximation using current client data. Each row explains its verdict. Regex uses Qt/PCRE, which can differ from Hyprland; unknown conditions are not treated as matches.")}};
}

QVariantMap Backend::simulateAppHints(const QVariantMap &settings, const QVariantList &apps)
{
    QVariantList matches, unaffected, uncertain;
    for (const auto &entry : apps) {
        const auto app = entry.toMap();
        const QString hint = app.value("class").toString();
        const bool inferred = app.value("inferred", true).toBool();
        const QString source = inferred ? tr("Desktop filename guess: %1").arg(hint)
                                        : tr("Declared StartupWMClass: %1").arg(hint);
        const auto verdict = matchWindow(hint.isEmpty() ? QVariantMap{} : QVariantMap{{"class", hint}}, settings);
        const QString reason = tr("%1. %2").arg(source, verdict.value("reason").toString());
        const QVariantMap row{{"name", app.value("name")}, {"reason", reason}};
        if (inferred || verdict.value("unknown").toBool())
            uncertain.append(row);
        else if (verdict.value("matched").toBool())
            matches.append(row);
        else
            unaffected.append(row);
    }
    return {{"matches", matches}, {"unaffected", unaffected}, {"uncertain", uncertain}};
}

QVariantMap Backend::interpretRuleRequest(const QString &request) const
{
    const auto fail = [](const QString &message) { return QVariantMap{{"error", message}}; };
    if (request.size() > 240) return fail(tr("Use one request of at most 240 characters."));
    const auto window = selectedWindow();
    const auto klass = window.value("currentClass").toString();
    if (klass.trimmed().isEmpty() || window.value("address").toString().isEmpty())
        return fail(tr("Select an open window so “this app” has an observed class."));

    QString remaining = request.simplified();
    if (remaining.endsWith('.')) remaining.chop(1);
    const auto consume = [&](const QString &pattern) {
        const QRegularExpression expression("\\A(?:" + pattern + ")(?= |$)",
            QRegularExpression::CaseInsensitiveOption);
        const auto match = expression.match(remaining);
        if (match.hasMatch()) remaining = remaining.mid(match.capturedLength()).trimmed();
        return match;
    };
    if (!consume("open this app").hasMatch() || remaining.isEmpty())
        return fail(tr("Start with “open this app” and describe a layout, size, or workspace."));

    QString layout, workspace;
    int width = 0, height = 0;
    bool hasSize = false, centered = false;
    while (!remaining.isEmpty()) {
        if (const auto match = consume("(floating|tiled)"); match.hasMatch()) {
            if (!layout.isEmpty()) return fail(tr("Specify just one layout."));
            layout = match.captured(1).toLower();
        } else if (const auto match = consume("at ([0-9]{1,5}) *[x×] *([0-9]{1,5})"); match.hasMatch()) {
            if (hasSize) return fail(tr("Specify just one window size."));
            hasSize = true;
            width = match.captured(1).toInt();
            height = match.captured(2).toInt();
        } else if (const auto match = consume("on workspace ([0-9]{1,5})"); match.hasMatch()) {
            if (!workspace.isEmpty()) return fail(tr("Specify just one workspace."));
            const int number = match.captured(1).toInt();
            if (number < 1) return fail(tr("Use a positive workspace number."));
            workspace = QString::number(number);
        } else if (consume("centered").hasMatch()) {
            if (centered) return fail(tr("Specify centering only once."));
            centered = true;
        } else {
            return fail(tr("Unsupported wording: “%1”. Nothing was staged.").arg(remaining));
        }
    }
    if (layout == "tiled" && (hasSize || centered))
        return fail(tr("A tiled window cannot be given a floating size or centered position."));
    if (layout.isEmpty()) layout = hasSize || centered ? "floating" : "keep";
    if (layout == "floating" && !hasSize) {
        const auto size = window.value("size").toList();
        if (size.size() != 2) return fail(tr("The live size is unavailable. Specify a size such as “at 800×600”."));
        width = size[0].toInt();
        height = size[1].toInt();
    }

    QVariantMap settings{{"layout", layout}, {"center", centered}, {"workspace", workspace},
        {"matchTitle", false}, {"classMatchMode", "exact"}, {"match", QVariantMap{{"class", klass}}}};
    if (layout == "floating") {
        settings.insert("width", width);
        settings.insert("height", height);
    }
    const auto problems = validateSettings(settings);
    if (!problems.isEmpty()) return fail(problems.join("\n"));
    return {{"settings", settings}, {"summary", summarize(settings)}, {"address", window.value("address")},
        {"class", klass}, {"usesLiveSize", layout == "floating" && !hasSize}};
}

bool Backend::geometrySuggestionsEnabled() const
{
    return preference("geometrySuggestions", false).toBool();
}

bool Backend::setGeometrySuggestionsEnabled(bool enabled)
{
    if (enabled == geometrySuggestionsEnabled()) return true;
    auto library = m_library;
    auto preferences = library.value("preferences").toObject();
    preferences.insert("geometrySuggestions", enabled);
    library.insert("preferences", preferences);
    if (!writeLibrary(library)) return false;
    m_dismissedGeometryRules.clear();
    resetGeometryObservations();
    return true;
}

void Backend::resetGeometryObservations()
{
    m_geometryObservations.clear();
    if (m_geometrySuggestion.isEmpty()) return;
    m_geometrySuggestion.clear();
    emit geometrySuggestionChanged();
}

void Backend::dismissGeometrySuggestion()
{
    if (m_geometrySuggestion.isEmpty()) return;
    m_dismissedGeometryRules.insert(m_geometrySuggestion.value("id").toString());
    m_geometrySuggestion.clear();
    emit geometrySuggestionChanged();
}

void Backend::observeGeometryChanges()
{
    if (!geometrySuggestionsEnabled() || trialActive() || monitorRevertPending()) {
        resetGeometryObservations();
        return;
    }
    constexpr int threshold = 32;
    QHash<QString, QVariantMap> observations;
    QVariantMap suggestion;
    for (const auto &value : m_windows) {
        const auto window = value.toMap();
        const QString address = window.value("address").toString();
        const auto size = window.value("size").toList(), at = window.value("at").toList();
        if (address.isEmpty() || !window.value("floating").toBool() || window.value("grouped").toBool()
            || window.value("fullscreen").toInt() || size.size() != 2 || at.size() != 2
            || window.value("workspace").toString().startsWith("special:"))
            continue;
        QVariantMap matchRule, settings;
        int ruleIndex = -1;
        for (int i = 0; i < m_savedRules.size(); ++i) {
            const auto rule = m_savedRules.at(i).toMap();
            if (!rule.value("enabled", true).toBool()) continue;
            auto candidate = rule.value("settings").toMap();
            candidate.insert("match", rule.value("match"));
            const auto verdict = matchWindow(window, candidate);
            if (verdict.value("unknown").toBool() ||
                (ruleIndex >= 0 && verdict.value("matched").toBool())) {
                ruleIndex = -1;
                break;
            }
            if (!verdict.value("matched").toBool()) continue;
            matchRule = rule;
            settings = candidate;
            ruleIndex = i;
        }
        const QString id = matchRule.value("id").toString();
        if (ruleIndex < 0 || id.isEmpty() || m_dismissedGeometryRules.contains(id)
            || settings.value("layout", "floating").toString() != "floating"
            || settings.value("placement").toMap().value("enabled").toBool())
            continue;
        const int w = size[0].toInt(), h = size[1].toInt(), x = at[0].toInt(), y = at[1].toInt();
        const bool tracksPosition = !settings.value("center", true).toBool() && settings.value("setPosition").toBool();
        const bool resized = qAbs(w - settings.value("width", 600).toInt()) >= threshold
            || qAbs(h - settings.value("height", 420).toInt()) >= threshold;
        const bool moved = tracksPosition && (qAbs(x - settings.value("x").toInt()) >= threshold
            || qAbs(y - settings.value("y").toInt()) >= threshold);
        const QVariantList context{id, window.value("pid"), window.value("workspace"),
            window.value("monitor"), window.value("monitorName")};
        const auto previous = m_geometryObservations.value(address);
        const bool sameContext = previous.value("context").toList() == context;
        const bool sizeChanged = sameContext && (qAbs(w - previous.value("w").toInt()) >= threshold
            || qAbs(h - previous.value("h").toInt()) >= threshold);
        const bool positionChanged = sameContext && (qAbs(x - previous.value("x").toInt()) >= threshold
            || qAbs(y - previous.value("y").toInt()) >= threshold);
        const int resizeCount = sameContext ? previous.value("resizes").toInt() + int(resized && sizeChanged) : 0;
        const int moveCount = sameContext ? previous.value("moves").toInt() + int(moved && positionChanged) : 0;
        observations.insert(address, QVariantMap{{"context", context}, {"resizes", resizeCount}, {"moves", moveCount},
            {"w", w}, {"h", h}, {"x", x}, {"y", y}});
        QVariantMap patch;
        if (resized && resizeCount >= 2) { patch.insert("width", w); patch.insert("height", h); }
        if (moved && moveCount >= 2) { patch.insert("x", x); patch.insert("y", y); }
        if (patch.isEmpty() || !suggestion.isEmpty()) continue;
        for (auto it = patch.cbegin(); it != patch.cend(); ++it) settings.insert(it.key(), it.value());
        if (!validateSettings(settings).isEmpty()) continue;
        suggestion = {{"id", id}, {"name", matchRule.value("name", matchRule.value("class"))},
            {"index", ruleIndex}, {"settings", patch}};
    }
    m_geometryObservations = observations;
    if (suggestion != m_geometrySuggestion) {
        m_geometrySuggestion = suggestion;
        emit geometrySuggestionChanged();
    }
}

QVariantMap Backend::inspectWindow() const
{
    const auto window = selectedWindow();
    QVariantList matching;
    QStringList conflicts;
    QVariantList conflictMap;
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
    }
    for (int i = 0; i < matching.size(); ++i) {
        const QVariantMap first = matching.at(i).toMap();
        if (first.value("unknown").toBool()) continue;
        const QVariantMap firstEffects = first.value("effects").toMap();
        for (int j = i + 1; j < matching.size(); ++j) {
            const QVariantMap second = matching.at(j).toMap();
            if (second.value("unknown").toBool()) continue;
            const QVariantMap secondEffects = second.value("effects").toMap();
            for (auto it = firstEffects.cbegin(); it != firstEffects.cend(); ++it) {
                if (!secondEffects.contains(it.key()) ||
                    it.value().toString() == secondEffects.value(it.key()).toString())
                    continue;
                const QString firstName = first.value("name").toString();
                const QString secondName = second.value("name").toString();
                const QString firstValue = it.value().toString();
                const QString secondValue = secondEffects.value(it.key()).toString();
                conflicts << tr("%1: %2 requests “%3”; %4 requests “%5”.")
                    .arg(it.key(), firstName, firstValue, secondName, secondValue);
                conflictMap.append(QVariantMap{{"setting", it.key()},
                    {"firstRule", firstName}, {"firstValue", firstValue},
                    {"secondRule", secondName}, {"secondValue", secondValue}});
            }
        }
    }
    return {{"window", window}, {"rules", matching}, {"conflicts", conflicts},
        {"conflictMap", conflictMap}, {"externalHints", externalRuleHints()},
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
