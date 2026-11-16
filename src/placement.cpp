#include "backend.h"
#include <QtMath>
#include <cmath>

QVariantMap Backend::placementGeometry(const QVariantMap &p, const QVariantList &monitors)
{
    if (!p.value("enabled").toBool()) return {};
    QVariantMap monitor;
    for (const auto &entry : monitors)
        if (entry.toMap().value("name") == p.value("monitor")) { monitor = entry.toMap(); break; }
    bool fallback = monitor.isEmpty();
    if (fallback) {
        for (const auto &entry : monitors)
            if (entry.toMap().value("name") == p.value("fallback")) { monitor = entry.toMap(); break; }
    }
    if (monitor.isEmpty() && !monitors.isEmpty()) monitor = monitors.first().toMap();
    if (monitor.isEmpty()) return {{"error", tr("No connected monitor geometry available.")}};
    const double scale = monitor.value("scale", 1).toDouble();
    if (!std::isfinite(scale) || scale <= 0) return {{"error", tr("Invalid monitor scale.")}};
    int physicalW = monitor.value("width").toInt(), physicalH = monitor.value("height").toInt();
    if (monitor.value("transform").toInt() % 2) qSwap(physicalW, physicalH);
    const auto reserved = monitor.value("reserved").toList();
    const int left = reserved.size() == 4 ? reserved[0].toInt() : 0;
    const int top = reserved.size() == 4 ? reserved[1].toInt() : 0;
    const int right = reserved.size() == 4 ? reserved[2].toInt() : 0;
    const int bottom = reserved.size() == 4 ? reserved[3].toInt() : 0;
    const int usableW = qFloor(physicalW / scale) - left - right;
    const int usableH = qFloor(physicalH / scale) - top - bottom;
    if (usableW <= 0 || usableH <= 0) return {{"error", tr("Monitor has no usable area.")}};
    const int margin = qBound(0, p.value("margin", 24).toInt(), qMax(0, qMin(usableW, usableH) / 2 - 1));
    const int availableW = qMax(1, usableW - margin * 2), availableH = qMax(1, usableH - margin * 2);
    const bool percentage = p.value("unit", "percent") == "percent";
    const double requestedW = p.value("width", percentage ? 60 : 600).toDouble();
    const double requestedH = p.value("height", percentage ? 60 : 420).toDouble();
    if (!std::isfinite(requestedW) || !std::isfinite(requestedH) || requestedW <= 0 || requestedH <= 0 ||
        (percentage && (requestedW > 100 || requestedH > 100)))
        return {{"error", tr("Placement dimensions must be positive; percentages cannot exceed 100.")}};
    int w = qBound(1, qRound(percentage ? usableW * requestedW / 100 : requestedW), availableW);
    int h = qBound(1, qRound(percentage ? usableH * requestedH / 100 : requestedH), availableH);
    if (p.value("aspectLock").toBool()) {
        const double ratio = p.value("aspect", 1.5).toDouble();
        if (!std::isfinite(ratio) || ratio <= 0) return {{"error", tr("Aspect ratio must be positive.")}};
        h = qMax(1, qRound(w / ratio));
        if (h > availableH) { h = availableH; w = qMax(1, qRound(h * ratio)); }
    }
    const QString anchor = p.value("anchor", "center").toString();
    int x = left + (usableW - w) / 2, y = top + (usableH - h) / 2;
    if (anchor.contains("left")) x = left + margin;
    if (anchor.contains("right")) x = left + usableW - w - margin;
    if (anchor.contains("top")) y = top + margin;
    if (anchor.contains("bottom")) y = top + usableH - h - margin;
    if (anchor == "custom") {
        x = qBound(left, p.value("x", left).toInt(), left + usableW - w);
        y = qBound(top, p.value("y", top).toInt(), top + usableH - h);
    }
    return {{"layout", "floating"}, {"center", false}, {"setPosition", true},
            {"width", w}, {"height", h}, {"x", x}, {"y", y},
            {"monitor", monitor.value("name")}, {"usedFallback", fallback},
            {"usableX", left}, {"usableY", top}, {"usableWidth", usableW}, {"usableHeight", usableH},
            {"monitorWidth", qFloor(physicalW / scale)}, {"monitorHeight", qFloor(physicalH / scale)}};
}

QVariantMap Backend::resolvePlacement(const QVariantMap &settings) const
{
    QVariantMap result = settings;
    const auto p = settings.value("placement").toMap();
    const auto geometry = placementGeometry(p, m_monitors);
    for (auto it = geometry.cbegin(); it != geometry.cend(); ++it) result.insert(it.key(), it.value());
    return result;
}
