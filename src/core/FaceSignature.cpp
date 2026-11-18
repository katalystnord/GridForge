#include "core/FaceSignature.h"

#include <QJsonArray>

#include <cmath>

namespace gf::core {

bool sameFace(const FaceSignature &was, const FaceSignature &now)
{
    if (was.kind != now.kind)
        return false;
    if (std::abs(now.area - was.area) > 1e-6 * was.area)
        return false;
    const double dx = now.centre[0] - was.centre[0], dy = now.centre[1] - was.centre[1],
                 dz = now.centre[2] - was.centre[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz) <= 1e-6 * std::sqrt(was.area);
}

bool sameSolid(const SolidSignature &was, const SolidSignature &now)
{
    if (std::abs(now.volume - was.volume) > 1e-6 * was.volume)
        return false;
    const double dx = now.centre[0] - was.centre[0], dy = now.centre[1] - was.centre[1],
                 dz = now.centre[2] - was.centre[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz) <= 1e-6 * std::cbrt(was.volume);
}

QString describeFace(const FaceSignature &s, const QString &length)
{
    auto n = [](double x) { return QString::number(std::abs(x) < 1e-12 ? 0.0 : x, 'g', 6); };
    const QString kind = s.kind.isEmpty() || s.kind == "other" ? QString("surface of another kind") : s.kind;
    const QString article = QString("aeiou").contains(kind.front()) ? "an" : "a";
    return QString("%1 %2 of %3 %4%5 at (%6, %7, %8)")
        .arg(article, kind, n(s.area), length, "2", n(s.centre[0]), n(s.centre[1]), n(s.centre[2]));
}

QJsonObject faceSignatureJson(const FaceSignature &s)
{
    return QJsonObject{{"kind", s.kind}, {"area", s.area}, {"centre", QJsonArray{s.centre[0], s.centre[1], s.centre[2]}}};
}

} // namespace gf::core
