// A CalculiX job's deck from a study's set-up (study spec, section 4): the
// groups as sets of GridForge's mesh, then the analysis deck around it.
// The spec's example on the box must give the deck that the hand-written
// fixtures/calculix/box-gfmesh/tension.inp is (by group names, not face
// numbers); ccx solving it is fixture box-setup (test_mesh_to_ccx).

#include "calculix/DeckWriter.h"
#include "core/Setup.h"
#include "mesh/Groups.h"
#include "mesh/Mesher.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>
#include <cstdio>
#include <functional>
#include <set>
#include <vector>

using namespace gf::calculix;
using gf::core::NamedSet;
using gf::core::ResultsSeries;
using gf::core::Setup;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
#endif

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

// The spec's example, the material's values given in the study (4.8).
const char *example = R"({
  "units": "mm-N-s",
  "groups": {
    "Steel part": {"solids": [1]},
    "Back":   {"faces": [1]},
    "Side":   {"faces": [3]},
    "Bottom": {"faces": [5]},
    "Pulled": {"faces": [2]}
  },
  "materials": {
    "Steel": {"values": {"YoungsModulus": 210000, "PoissonRatio": 0.3}}
  },
  "assign": {"Steel part": "Steel"},
  "supports": [
    {"group": "Back",   "symmetry": true},
    {"group": "Side",   "symmetry": true},
    {"group": "Bottom", "symmetry": true}
  ],
  "steps": [
    {"name": "Pull", "kind": "static",
     "loads": [{"group": "Pulled", "displacement": {"x": 0.01}}],
     "output": ["displacement", "reaction", "stress", "strain"]}
  ]
})";

Setup setup(const QJsonObject &o)
{
    QString error;
    const Setup s = gf::core::readSetup(o, &error);
    if (!error.isEmpty())
        std::printf("  (set-up: %s)\n", qPrintable(error));
    return s;
}
QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }

ResultsSeries mesh(const char *file, double size = 5)
{
    QString error;
    ResultsSeries m = gf::mesh::meshStep(QString(GF_FIXTURES) + "/step/" + file, {size, 2}, &error);
    if (!error.isEmpty())
        std::printf("  (%s: %s)\n", file, qPrintable(error));
    return m;
}

const NamedSet *setOf(const ResultsSeries &s, const QString &name, bool nodes)
{
    for (const NamedSet &n : s.sets)
        if (n.name == name && n.nodes == nodes)
            return &n;
    return nullptr;
}

// The deck for the example with one change to its JSON, on the box.
QString deckWith(const std::function<void(QJsonObject &)> &change, QString *error, const char *file = "box.step")
{
    QJsonObject o = json(example);
    change(o);
    const Setup s = setup(o);
    ResultsSeries m = mesh(file);
    if (!gf::mesh::addGroupSets(m, s, error))
        return {};
    return analysisInp(s, m, "mesh.inp", error);
}

QStringList body(const QString &deck)
{
    QStringList out;
    for (const QString &l : deck.split('\n'))
        if (!l.startsWith("**") && !l.isEmpty())
            out << l;
    return out;
}

} // namespace

CASE(groups_become_sets_named_as_the_groups)
{
    const Setup s = setup(json(example));
    ResultsSeries m = mesh("box.step");
    QString error;
    CHECK(gf::mesh::addGroupSets(m, s, &error) && error.isEmpty());
    const NamedSet *back = setOf(m, "Back", true), *face1 = setOf(m, "Face1", true);
    CHECK(back && face1 && back->members == face1->members);
    const NamedSet *part = setOf(m, "Steel part", false), *solid = setOf(m, "Solid1", false);
    CHECK(part && solid && part->members == solid->members);
    CHECK(setOf(m, "Steel part", true) && !setOf(m, "Back", false));
}

CASE(a_group_of_several_faces_holds_the_nodes_of_all)
{
    QJsonObject o = json(example);
    QJsonObject groups = o["groups"].toObject();
    groups["Ends"] = QJsonObject{{"faces", QJsonArray{1, 2}}};
    o["groups"] = groups;
    ResultsSeries m = mesh("box.step");
    QString error;
    CHECK(gf::mesh::addGroupSets(m, setup(o), &error));
    const NamedSet *ends = setOf(m, "Ends", true), *a = setOf(m, "Face1", true), *b = setOf(m, "Face2", true);
    CHECK(ends && a && b && ends->members.size() == a->members.size() + b->members.size());
    CHECK(ends && std::is_sorted(ends->members.begin(), ends->members.end()));
}

CASE(a_face_the_mesh_does_not_have_is_refused)
{
    QString error;
    deckWith([](QJsonObject &o) {
        QJsonObject g = o["groups"].toObject();
        g["Back"] = QJsonObject{{"faces", QJsonArray{9}}};
        o["groups"] = g;
    }, &error);
    CHECK(error == "group \"Back\": face 9; the geometry has 6");
}

CASE(the_specs_example_gives_the_hand_written_deck)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &) {}, &error);
    CHECK(error.isEmpty());
    const QStringList expected = {
        "*INCLUDE, INPUT=mesh.inp",
        "*MATERIAL, NAME=Steel",
        "*ELASTIC",
        "210000, 0.3",
        "*SOLID SECTION, ELSET=Steel_part, MATERIAL=Steel",
        "*BOUNDARY",
        "Back, 1, 1, 0",
        "Side, 2, 2, 0",
        "Bottom, 3, 3, 0",
        "*STEP",
        "*STATIC",
        "*BOUNDARY",
        "Pulled, 1, 1, 0.01",
        "*NODE FILE",
        "U, RF",
        "*EL FILE",
        "S, E",
        "*END STEP",
    };
    CHECK(body(deck) == expected);
    if (body(deck) != expected)
        std::printf("%s\n", qPrintable(deck));
    const QStringList l = deck.split('\n');
    CHECK(l.value(0).startsWith("** written by GridForge "));
    CHECK(l.contains("** units: mm-N-s (lengths mm, forces N, stresses MPa)"));
    CHECK(l.contains("** material Steel: YoungsModulus 210000, PoissonRatio 0.3, entered in the study"));
    CHECK(l.contains("** step Pull"));
}

CASE(fixed_and_given_movements_are_written_by_direction)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &o) {
        o["supports"] = QJsonDocument::fromJson(R"([{"group": "Back", "fixed": true},
            {"group": "Side", "displacement": {"y": 0, "z": -0.5}}])").array();
    }, &error);
    CHECK(error.isEmpty());
    const QStringList b = body(deck);
    CHECK(b.contains("Back, 1, 3, 0"));
    CHECK(b.contains("Side, 2, 2, 0") && b.contains("Side, 3, 3, -0.5"));
}

CASE(symmetry_on_a_face_not_normal_to_an_axis_is_refused)
{
    // The dogbone's face 2 is one of its R30 transitions, curved.
    QString error;
    deckWith([](QJsonObject &o) {
        QJsonObject g = o["groups"].toObject();
        g["Back"] = QJsonObject{{"faces", QJsonArray{2}}};
        o["groups"] = g;
    }, &error, "dogbone.step");
    CHECK(error == "supports[0]: group \"Back\" is not a plane normal to x, y or z; symmetry needs one "
                   "in version 1");
}

CASE(a_solid_with_no_material_is_refused)
{
    QString error;
    deckWith([](QJsonObject &) {}, &error, "twoboxes.step");
    CHECK(error == "solid 2 has no material");
}

CASE(a_material_without_an_elastic_value_is_refused)
{
    QString error;
    deckWith([](QJsonObject &o) {
        o["materials"] = QJsonDocument::fromJson(R"({"Steel": {"values": {"YoungsModulus": 210000}}})").object();
    }, &error);
    CHECK(error == "material Steel: no PoissonRatio given in the study (reading cards is not written yet)");
}

CASE(an_output_not_known_is_refused)
{
    QString error;
    deckWith([](QJsonObject &o) {
        QJsonArray steps = o["steps"].toArray();
        QJsonObject st = steps[0].toObject();
        st["output"] = QJsonArray{"displacement", "vorticity"};
        steps[0] = st;
        o["steps"] = steps;
    }, &error);
    CHECK(error == "step Pull: output \"vorticity\" is not one of displacement, reaction, stress, strain, "
                   "plastic strain");
}

CASE(a_plastic_material_is_included_and_turns_on_large_displacements)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &o) {
        o["materials"] = QJsonDocument::fromJson(R"({"Steel": {"plastic": "inputs/S355-plastic.inp",
            "values": {"YoungsModulus": 210000, "PoissonRatio": 0.3}}})").object();
    }, &error);
    CHECK(error.isEmpty());
    const QStringList b = body(deck), l = deck.split('\n');
    CHECK(b.contains("*INCLUDE, INPUT=inputs/S355-plastic.inp"));
    CHECK(b.contains("*STEP, NLGEOM"));
    CHECK(l.contains("** step Pull: large displacements (NLGEOM), turned on by itself: Steel is plastic"));
}

CASE(a_step_may_keep_large_displacements_off)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &o) {
        o["materials"] = QJsonDocument::fromJson(R"({"Steel": {"plastic": "inputs/S355-plastic.inp",
            "values": {"YoungsModulus": 210000, "PoissonRatio": 0.3}}})").object();
        QJsonArray steps = o["steps"].toArray();
        QJsonObject st = steps[0].toObject();
        st["nonlinear geometry"] = false;
        steps[0] = st;
        o["steps"] = steps;
    }, &error);
    CHECK(body(deck).contains("*STEP") && !body(deck).contains("*STEP, NLGEOM"));
}

CASE(raw_lines_are_written_into_their_step_and_marked)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &o) {
        QJsonArray steps = o["steps"].toArray();
        QJsonObject st = steps[0].toObject();
        st["raw"] = QJsonArray{"*CONTROLS, PARAMETERS=TIME INCREMENTATION", "4, 8, 9, 200, 10, 4, 0, 10"};
        steps[0] = st;
        o["steps"] = steps;
    }, &error);
    const QStringList l = deck.split('\n');
    const int at = l.indexOf("** given by hand in the study:");
    CHECK(at > 0 && l.value(at + 1) == "*CONTROLS, PARAMETERS=TIME INCREMENTATION"
          && l.value(at + 2) == "4, 8, 9, 200, 10, 4, 0, 10" && l.value(at + 3) == "*END STEP");
}

CASE(a_step_that_replaces_is_refused_until_written)
{
    QString error;
    deckWith([](QJsonObject &o) {
        QJsonArray steps = o["steps"].toArray();
        QJsonObject st = steps[0].toObject();
        st["replace"] = true;
        steps[0] = st;
        o["steps"] = steps;
    }, &error);
    CHECK(error == "step Pull: replacing what came before is not written yet");
}

int main()
{
    for (auto &[name, run] : cases()) {
        const int before = failures;
        run();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
