// The window's support and load form, and the study.json entry it stands
// for (study spec 4.6; docs/survey/loads-and-supports.md): both ways, and
// what cannot be written refused in words.

#include "core/LoadChoice.h"

#include <QJsonDocument>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::core;

namespace {

int failures = 0;
std::vector<std::pair<const char *, std::function<void()>>> &cases()
{
    static std::vector<std::pair<const char *, std::function<void()>>> c;
    return c;
}
struct Register {
    Register(const char *n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
#define CASE(name)                                  \
    void name();                                    \
    static Register reg_##name(#name, name);        \
    void name()
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }

LoadChoice choice(LoadChoice::Kind kind, const QString &group = "Grip")
{
    LoadChoice c;
    c.kind = kind;
    c.group = group;
    return c;
}

Direction axis(int a, bool reversed = false)
{
    Direction d;
    d.from = Direction::From::Axis;
    d.axis = a;
    d.reversed = reversed;
    return d;
}

} // namespace

CASE(a_direction_is_written_and_read_in_words)
{
    CHECK(directionText(axis(2)) == "+z");
    CHECK(directionText(axis(0, true)) == "-x");
    Direction n;
    n.from = Direction::From::Normal;
    n.number = 6;
    CHECK(directionText(n) == "outward normal of face 6");
    n.reversed = true;
    CHECK(directionText(n) == "inward normal of face 6");
    Direction e;
    e.from = Direction::From::Edge;
    e.number = 14;
    CHECK(directionText(e) == "along edge 14");
    e.reversed = true;
    CHECK(directionText(e) == "against edge 14");
    CHECK(directionText(Direction{}).isEmpty());  // given by components

    for (const char *text : {"+x", "-y", "+z", "outward normal of face 6", "inward normal of face 12",
                             "along edge 14", "against edge 3"}) {
        const auto d = parseDirection(text);
        CHECK(d && directionText(*d) == text);
    }
    const auto d = parseDirection("-y");
    CHECK(d && d->from == Direction::From::Axis && d->axis == 1 && d->reversed);
    for (const char *text : {"", "z", "sideways", "+w", "outward normal of face 0", "along edge x",
                             "normal of face 6", "along edge 14 "})
        CHECK(!parseDirection(text));
}

CASE(bending_the_dogbone_is_a_move_along_z)
{
    // Pick the free grip, Move, Z, 2 mm: the others free (the spec's default).
    LoadChoice c = choice(LoadChoice::Kind::Move);
    c.direction = axis(2);
    c.magnitude = 2;
    QString error;
    QJsonObject e = entryOf(c, &error);
    CHECK(error.isEmpty());
    CHECK(e == json(R"({"group": "Grip", "displacement": {"z": 2}, "direction": "+z"})"));
    // Reversed, the others held where they are.
    c.direction.reversed = true;
    c.othersHeld = true;
    e = entryOf(c, &error);
    CHECK(e == json(R"({"group": "Grip", "displacement": {"x": 0, "y": 0, "z": -2}, "direction": "-z"})"));
}

CASE(a_force_is_one_magnitude_in_a_direction)
{
    LoadChoice c = choice(LoadChoice::Kind::Force);
    c.direction = axis(1, true);
    c.magnitude = 50;
    QString error;
    CHECK(entryOf(c, &error) == json(R"({"group": "Grip", "force": {"y": -50}, "direction": "-y"})"));
    // Along a face's normal, measured on the geometry: 45 degrees in x-z.
    c.direction = Direction{};
    c.direction.from = Direction::From::Normal;
    c.direction.number = 6;
    const double h = std::sqrt(0.5);
    c.unit = {h, 0, h};
    c.magnitude = 100;
    QJsonObject e = entryOf(c, &error);
    CHECK(error.isEmpty() && e["direction"].toString() == "outward normal of face 6");
    const QJsonObject f = e["force"].toObject();
    CHECK(std::abs(f["x"].toDouble() - 100 * h) < 1e-12 && std::abs(f["z"].toDouble() - 100 * h) < 1e-12);
    CHECK(!f.contains("y"));  // a component that is nothing is not written
    // Inward: the other way.
    c.direction.reversed = true;
    e = entryOf(c, &error);
    CHECK(e["force"].toObject()["x"].toDouble() < 0 && e["direction"].toString() == "inward normal of face 6");
    // By components, as typed.
    c.direction = Direction{};
    c.components = {0, 30, -40};
    CHECK(entryOf(c, &error) == json(R"({"group": "Grip", "force": {"y": 30, "z": -40}})"));
}

CASE(supports_are_held_and_released_per_direction)
{
    QString error;
    CHECK(entryOf(choice(LoadChoice::Kind::Fixed), &error) == json(R"({"group": "Grip", "fixed": true})"));
    CHECK(entryOf(choice(LoadChoice::Kind::Symmetry), &error) == json(R"({"group": "Grip", "symmetry": true})"));
    LoadChoice c = choice(LoadChoice::Kind::Axes);
    c.axes[0] = {AxisState::Kind::Held, 0};
    c.axes[1] = {AxisState::Kind::Free, 0};
    c.axes[2] = {AxisState::Kind::Value, 0.5};
    CHECK(entryOf(c, &error) == json(R"({"group": "Grip", "displacement": {"x": 0, "z": 0.5}})"));
    // Every direction held is the fixed support, written as such.
    c.axes[1] = c.axes[2] = {AxisState::Kind::Held, 0};
    CHECK(entryOf(c, &error) == json(R"({"group": "Grip", "fixed": true})"));
}

CASE(what_cannot_be_written_is_refused_in_words)
{
    QString error;
    LoadChoice c = choice(LoadChoice::Kind::Axes);
    CHECK(entryOf(c, &error).isEmpty() && error == "every direction is free, so it holds nothing");
    c = choice(LoadChoice::Kind::Force);
    c.direction = axis(2);
    c.magnitude = 0;
    CHECK(entryOf(c, &error).isEmpty() && error == "a force of 0 does nothing");
    // A movement along a direction off x, y and z needs a local system,
    // which version 1 does not write.
    c = choice(LoadChoice::Kind::Move);
    c.direction.from = Direction::From::Normal;
    c.direction.number = 6;
    const double h = std::sqrt(0.5);
    c.unit = {h, 0, h};
    c.magnitude = 1;
    CHECK(entryOf(c, &error).isEmpty()
          && error == "face 6's normal is not along x, y or z; a movement along it is not written yet, "
                      "so give it by components");
    // Along x after all (as measured, to rounding): written on x.
    c.unit = {1, 1e-13, 0};
    CHECK(entryOf(c, &error) == json(R"({"group": "Grip", "displacement": {"x": 1}, "direction": "outward normal of face 6"})"));
    c = choice(LoadChoice::Kind::Force, "");
    c.direction = axis(0);
    c.magnitude = 1;
    CHECK(entryOf(c, &error).isEmpty() && error == "no group");
}

CASE(an_entry_is_shown_as_the_choice_that_made_it)
{
    // The dogbone's pull: along x, the others held.
    LoadChoice c = choiceOf(json(R"({"group": "Pulled grip", "displacement": {"x": 1.5, "y": 0, "z": 0}})"));
    CHECK(c.kind == LoadChoice::Kind::Move && c.group == "Pulled grip");
    CHECK(c.direction.from == Direction::From::Axis && c.direction.axis == 0 && !c.direction.reversed);
    CHECK(c.magnitude == 1.5 && c.othersHeld);
    // Mixed: given by components.
    c = choiceOf(json(R"({"group": "G", "displacement": {"x": 1.5, "y": 0}})"));
    CHECK(c.kind == LoadChoice::Kind::Move && c.direction.from == Direction::From::Components);
    CHECK(c.axes[0].kind == AxisState::Kind::Value && c.axes[0].value == 1.5);
    CHECK(c.axes[1].kind == AxisState::Kind::Held && c.axes[2].kind == AxisState::Kind::Free);
    c = choiceOf(json(R"({"group": "G", "force": {"z": -50}, "direction": "-z"})"));
    CHECK(c.kind == LoadChoice::Kind::Force && c.direction.axis == 2 && c.direction.reversed && c.magnitude == 50);
    c = choiceOf(json(R"({"group": "G", "force": {"x": 3, "z": 4}, "direction": "inward normal of face 6"})"));
    CHECK(c.direction.from == Direction::From::Normal && c.direction.reversed && c.magnitude == 5);
    CHECK(std::abs(c.unit.x + 0.6) < 1e-12 && std::abs(c.unit.z + 0.8) < 1e-12);  // outward: the other way
    c = choiceOf(json(R"({"group": "G", "force": {"x": 3, "z": 4}})"));
    CHECK(c.direction.from == Direction::From::Components && c.components.x == 3 && c.components.z == 4);
    CHECK(choiceOf(json(R"({"group": "G", "fixed": true})")).kind == LoadChoice::Kind::Fixed);
    CHECK(choiceOf(json(R"({"group": "G", "symmetry": true})")).kind == LoadChoice::Kind::Symmetry);
    c = choiceOf(json(R"({"group": "G", "pressure": 5})"));
    CHECK(c.kind == LoadChoice::Kind::Pressure && c.magnitude == 5);
    // A support given by direction (in the supports, not a step's loads).
    c = choiceOf(json(R"({"group": "G", "displacement": {"y": 0}})"), true);
    CHECK(c.kind == LoadChoice::Kind::Axes && c.axes[1].kind == AxisState::Kind::Held
          && c.axes[0].kind == AxisState::Kind::Free);
    // Every choice written and read back is itself.
    for (const char *entry : {R"({"group": "G", "displacement": {"z": 2}, "direction": "+z"})",
                              R"({"group": "G", "force": {"y": -50}, "direction": "-y"})",
                              R"({"group": "G", "pressure": 5})", R"({"group": "G", "fixed": true})"}) {
        QString error;
        CHECK(entryOf(choiceOf(json(entry)), &error) == json(entry));
    }
}

int main()
{
    for (const auto &[name, run] : cases()) {
        const int before = failures;
        run();
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", name);
    }
    return failures == 0 ? 0 : 1;
}
