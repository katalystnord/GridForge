#include "core/LoadChoice.h"

#include <cmath>

namespace gf::core {

namespace {

const char *const axisName[3] = {"x", "y", "z"};

double at(const Vec3 &v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }

Vec3 scaled(const Vec3 &v, double s) { return {v.x * s, v.y * s, v.z * s}; }

double length(const Vec3 &v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// The axis a measured direction lies on, to rounding, and its sense: a
// face's normal or an edge as the geometry gives it is never exactly 1.
std::optional<std::pair<int, double>> onAxis(const Vec3 &u)
{
    constexpr double tolerance = 1e-9;
    for (int i = 0; i < 3; ++i) {
        const double a = at(u, i), b = at(u, (i + 1) % 3), c = at(u, (i + 2) % 3);
        if (std::abs(std::abs(a) - 1) <= tolerance && std::abs(b) <= tolerance && std::abs(c) <= tolerance)
            return std::pair{i, a > 0 ? 1.0 : -1.0};
    }
    return std::nullopt;
}

// What the direction is, in words for a refusal: "face 6's normal", "edge 14".
QString named(const Direction &d)
{
    return d.from == Direction::From::Normal ? QString("face %1's normal").arg(d.number)
                                             : QString("edge %1").arg(d.number);
}

// Free, held or moved, per direction; empty if every one is free.
QJsonObject perAxis(const AxisState (&axes)[3])
{
    QJsonObject o;
    for (int i = 0; i < 3; ++i) {
        if (axes[i].kind == AxisState::Kind::Held)
            o[axisName[i]] = 0;
        else if (axes[i].kind == AxisState::Kind::Value)
            o[axisName[i]] = axes[i].value;
    }
    return o;
}

// The components of a vector, those that are nothing (to rounding, against
// its size) left out.
QJsonObject components(const Vec3 &v)
{
    const double size = length(v);
    QJsonObject o;
    for (int i = 0; i < 3; ++i)
        if (std::abs(at(v, i)) > 1e-12 * size)
            o[axisName[i]] = at(v, i);
    return o;
}

Vec3 vectorOf(const QJsonObject &o) { return {o["x"].toDouble(), o["y"].toDouble(), o["z"].toDouble()}; }

} // namespace

QJsonObject entryOf(const LoadChoice &c, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return QJsonObject();
    };
    if (c.group.trimmed().isEmpty())
        return fail("no group");
    QJsonObject e{{"group", c.group}};
    const double sense = c.direction.reversed ? -1 : 1;
    switch (c.kind) {
    case LoadChoice::Kind::Fixed:
        e["fixed"] = true;
        break;
    case LoadChoice::Kind::Symmetry:
        e["symmetry"] = true;
        break;
    case LoadChoice::Kind::Axes: {
        const QJsonObject d = perAxis(c.axes);
        if (d.isEmpty())
            return fail("every direction is free, so it holds nothing");
        if (d.size() == 3 && d["x"].toDouble() == 0 && d["y"].toDouble() == 0 && d["z"].toDouble() == 0)
            e["fixed"] = true;
        else
            e["displacement"] = d;
        break;
    }
    case LoadChoice::Kind::Move: {
        if (c.direction.from == Direction::From::Components) {
            const QJsonObject d = perAxis(c.axes);
            if (d.isEmpty())
                return fail("every direction is free, so nothing is moved");
            e["displacement"] = d;
            break;
        }
        int axis = c.direction.axis;
        double sign = sense;
        if (c.direction.from != Direction::From::Axis) {
            // A movement is written per axis (ccx's *BOUNDARY); one off the
            // axes needs a local system, not written in version 1.
            const auto on = onAxis(c.unit);
            if (!on)
                return fail(named(c.direction) + " is not along x, y or z; a movement along it is not "
                                                 "written yet, so give it by components");
            axis = on->first;
            sign *= on->second;
        }
        QJsonObject d;
        for (int i = 0; i < 3; ++i)
            if (i == axis)
                d[axisName[i]] = sign * c.magnitude;
            else if (c.othersHeld)
                d[axisName[i]] = 0;
        e["displacement"] = d;
        e["direction"] = directionText(c.direction);
        break;
    }
    case LoadChoice::Kind::Force: {
        Vec3 v;
        if (c.direction.from == Direction::From::Components)
            v = c.components;
        else if (c.direction.from == Direction::From::Axis)
            v = scaled({c.direction.axis == 0 ? 1.0 : 0, c.direction.axis == 1 ? 1.0 : 0, c.direction.axis == 2 ? 1.0 : 0},
                       sense * c.magnitude);
        else
            v = scaled(c.unit, sense * c.magnitude);
        const QJsonObject f = components(v);
        if (f.isEmpty())
            return fail("a force of 0 does nothing");
        e["force"] = f;
        if (c.direction.from != Direction::From::Components)
            e["direction"] = directionText(c.direction);
        break;
    }
    case LoadChoice::Kind::Pressure:
        if (c.magnitude == 0)
            return fail("a pressure of 0 does nothing");
        e["pressure"] = c.magnitude;
        break;
    }
    if (error)
        error->clear();
    return e;
}

LoadChoice choiceOf(const QJsonObject &e, bool support)
{
    LoadChoice c;
    c.group = e["group"].toString();
    const std::optional<Direction> direction = parseDirection(e["direction"].toString());
    if (direction)
        c.direction = *direction;
    auto axesOf = [&](const QJsonObject &d) {
        for (int i = 0; i < 3; ++i) {
            if (!d.contains(axisName[i]))
                c.axes[i] = {AxisState::Kind::Free, 0};
            else if (d[axisName[i]].toDouble() == 0)
                c.axes[i] = {AxisState::Kind::Held, 0};
            else
                c.axes[i] = {AxisState::Kind::Value, d[axisName[i]].toDouble()};
        }
    };
    if (e["fixed"].toBool()) {
        c.kind = LoadChoice::Kind::Fixed;
    } else if (e["symmetry"].toBool()) {
        c.kind = LoadChoice::Kind::Symmetry;
    } else if (e.contains("pressure")) {
        c.kind = LoadChoice::Kind::Pressure;
        c.magnitude = e["pressure"].toDouble();
    } else if (e.contains("force")) {
        c.kind = LoadChoice::Kind::Force;
        const Vec3 v = vectorOf(e["force"].toObject());
        if (c.direction.from == Direction::From::Axis) {
            c.magnitude = std::abs(at(v, c.direction.axis));
        } else if (c.direction.from != Direction::From::Components) {
            c.magnitude = length(v);
            if (c.magnitude > 0)
                c.unit = scaled(v, (c.direction.reversed ? -1 : 1) / c.magnitude);
        } else {
            c.components = v;
        }
    } else if (support) {
        c.kind = LoadChoice::Kind::Axes;
        axesOf(e["displacement"].toObject());
    } else {
        c.kind = LoadChoice::Kind::Move;
        const QJsonObject d = e["displacement"].toObject();
        axesOf(d);
        // One direction moved, the others all free or all held: a movement
        // in a direction (the dogbone's pull: x 1.5, y 0, z 0).
        int moved = -1, held = 0, free = 0;
        for (int i = 0; i < 3; ++i) {
            if (c.axes[i].kind == AxisState::Kind::Value)
                moved = moved == -1 ? i : 3;
            held += c.axes[i].kind == AxisState::Kind::Held;
            free += c.axes[i].kind == AxisState::Kind::Free;
        }
        if (moved >= 0 && moved < 3 && (held == 2 || free == 2)) {
            const double value = c.axes[moved].value;
            c.magnitude = std::abs(value);
            c.othersHeld = held == 2;
            if (c.direction.from == Direction::From::Components || c.direction.from == Direction::From::Axis) {
                c.direction.from = Direction::From::Axis;
                c.direction.axis = moved;
                c.direction.reversed = value < 0;
            } else {
                Vec3 u;
                (moved == 0 ? u.x : moved == 1 ? u.y : u.z) = (value < 0 ? -1 : 1) * (c.direction.reversed ? -1 : 1);
                c.unit = u;
            }
        } else {
            c.direction = Direction{};
        }
    }
    return c;
}

} // namespace gf::core
