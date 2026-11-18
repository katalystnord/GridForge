#include "core/Direction.h"

#include <QRegularExpression>

namespace gf::core {

std::optional<Direction> parseDirection(const QString &text)
{
    static const QRegularExpression axis(R"(^([+-])([xyz])$)");
    static const QRegularExpression normal(R"(^(outward|inward) normal of face ([1-9][0-9]*)$)");
    static const QRegularExpression edge(R"(^(along|against) edge ([1-9][0-9]*)$)");
    Direction d;
    if (const auto m = axis.match(text); m.hasMatch()) {
        d.from = Direction::From::Axis;
        d.axis = m.captured(2)[0].unicode() - 'x';
        d.reversed = m.captured(1) == "-";
    } else if (const auto n = normal.match(text); n.hasMatch()) {
        d.from = Direction::From::Normal;
        d.number = n.captured(2).toInt();
        d.reversed = n.captured(1) == "inward";
    } else if (const auto e = edge.match(text); e.hasMatch()) {
        d.from = Direction::From::Edge;
        d.number = e.captured(2).toInt();
        d.reversed = e.captured(1) == "against";
    } else {
        return std::nullopt;
    }
    return d;
}

QString directionText(const Direction &d)
{
    switch (d.from) {
    case Direction::From::Axis:
        return QString("%1%2").arg(d.reversed ? '-' : '+').arg(QChar('x' + d.axis));
    case Direction::From::Normal:
        return QString("%1 normal of face %2").arg(d.reversed ? "inward" : "outward").arg(d.number);
    case Direction::From::Edge:
        return QString("%1 edge %2").arg(d.reversed ? "against" : "along").arg(d.number);
    case Direction::From::Components:
        break;
    }
    return {};
}

} // namespace gf::core
