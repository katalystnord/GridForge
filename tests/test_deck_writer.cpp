// A CalculiX job's deck from a study's set-up (study spec, section 4): the
// groups as sets of GridForge's mesh, then the analysis deck around it.
// The spec's example on the box must give the deck that the hand-written
// fixtures/calculix/box-gfmesh/tension.inp is (by group names, not face
// numbers); ccx solving it is fixture box-setup (test_mesh_to_ccx).

#include "calculix/DeckWriter.h"
#include "core/MaterialCard.h"
#include "core/Setup.h"
#include "mesh/Groups.h"
#include "mesh/Mesher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>
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
    Setup s = setup(o);
    if (!gf::core::fillFromCards(s, GF_FIXTURES, error))
        return {};
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

CASE(a_face_that_is_no_longer_what_its_group_was_made_on_is_said_never_guessed)
{
    // Face 1 of the box is the side x = 0: a plane of 600 mm2 at (0, 10, 15).
    auto signedAs = [](const char *kind, double area, double x) {
        return [=](QJsonObject &o) {
            QJsonObject g = o["groups"].toObject();
            g["Back"] = QJsonObject{
                {"faces", QJsonArray{1}},
                {"signatures", QJsonObject{{"1", QJsonObject{{"kind", kind}, {"area", area},
                                                              {"centre", QJsonArray{x, 10, 15}}}}}}};
            o["groups"] = g;
        };
    };
    QString error;
    CHECK(!deckWith(signedAs("plane", 600, 0), &error).isEmpty() && error.isEmpty());
    // Rounded as a STEP written again rounds: still the face.
    CHECK(!deckWith(signedAs("plane", 600 * (1 + 1e-9), 1e-9), &error).isEmpty() && error.isEmpty());
    // Not the face it was: said, with what it was and what it is now.
    CHECK(deckWith(signedAs("cylinder", 600, 0), &error).isEmpty());
    CHECK(error == "group \"Back\": face 1 is not the face the group was made on: it was a cylinder of 600 mm2 at "
                   "(0, 10, 15), and is now a plane of 600 mm2 at (0, 10, 15); pick the group's faces again, or keep "
                   "them as they are now");
    CHECK(deckWith(signedAs("plane", 606, 0), &error).isEmpty());
    CHECK(error.startsWith("group \"Back\": face 1 is not the face the group was made on: it was a plane of 606 mm2"));
    CHECK(deckWith(signedAs("plane", 600, 0.01), &error).isEmpty());
    CHECK(error.contains("it was a plane of 600 mm2 at (0.01, 10, 15)"));
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
    CHECK(l.contains("** material Steel: YoungsModulus 210000 (entered in the study), PoissonRatio 0.3 "
                     "(entered in the study)"));
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
    CHECK(error == "material Steel: no PoissonRatio given in the study, and it has no card");
    deckWith([](QJsonObject &o) {
        o["materials"] = QJsonDocument::fromJson(R"({"Steel": {"card": "materials/tensense-ro.FCMat"}})").object();
    }, &error);
    CHECK(error == "material Steel: no PoissonRatio, neither in the study nor in its card");
}

CASE(a_material_from_its_card_says_where_each_value_came_from)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &o) {
        o["materials"] = QJsonDocument::fromJson(R"({"Steel": {"card": "materials/Steel-Generic.FCMat",
            "values": {"YoungsModulus": 210000}}})").object();
    }, &error);
    CHECK(error.isEmpty());
    const QStringList b = body(deck), l = deck.split('\n');
    const int at = b.indexOf("*ELASTIC");
    CHECK(at > 0 && b.value(at + 1) == "210000, 0.3" && b.value(at + 2) == "*DENSITY"
          && b.value(at + 3) == "7.9e-09");
    CHECK(l.contains("** material Steel: YoungsModulus 210000 (entered in the study), PoissonRatio 0.3 "
                     "(0.3 in Steel-Generic.FCMat), Density 7.9e-09 (7900 kg/m^3 in Steel-Generic.FCMat)"));
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

// The example's displacement load replaced by `load` (JSON).
QString deckWithLoad(const char *load, QString *error, const char *density = nullptr)
{
    return deckWith([&](QJsonObject &o) {
        QJsonArray steps = o["steps"].toArray();
        QJsonObject st = steps[0].toObject();
        st["loads"] = QJsonArray{QJsonDocument::fromJson(QByteArray("[") + load + "]").array().at(0)};
        steps[0] = st;
        o["steps"] = steps;
        if (density) {
            QJsonObject materials = o["materials"].toObject(), steel = materials["Steel"].toObject();
            QJsonObject values = steel["values"].toObject();
            values["Density"] = QString(density).toDouble();
            steel["values"] = values;
            materials["Steel"] = steel;
            o["materials"] = materials;
        }
    }, error);
}

// The lines between `keyword` and the next keyword.
QStringList block(const QString &deck, const QString &keyword)
{
    const QStringList b = body(deck);
    QStringList out;
    for (int i = b.indexOf(keyword) + 1; i > 0 && i < b.size() && !b[i].startsWith("*"); ++i)
        out << b[i];
    return out;
}

CASE(a_force_is_spread_over_the_faces_nodes_and_sums_to_the_total)
{
    QString error;
    const QString deck = deckWithLoad(R"({"group": "Pulled", "force": {"x": 126000}})", &error);
    CHECK(error.isEmpty());
    const QStringList lines = block(deck, "*CLOAD");
    CHECK(!lines.isEmpty());
    // Every line on dof 1; their sum the total. Quadratic faces put it all
    // on the middle nodes (a third of each face's area to each), none on
    // the corners.
    ResultsSeries m = mesh("box.step");
    QString e;
    gf::mesh::addGroupSets(m, setup(json(example)), &e);
    const NamedSet *pulled = setOf(m, "Pulled", true);
    std::set<long long> corners;
    vtkUnstructuredGrid *g = m.points.front().grid;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c)
        for (int k = 0; k < 4; ++k)
            corners.insert(g->GetCell(c)->GetPointId(k) + 1);  // gf_mesh numbers nodes from 1 in order
    double sum = 0;
    bool onDof1 = true, onPulled = true, noCorner = true;
    for (const QString &line : lines) {
        const QStringList f = line.split(", ");
        onDof1 = onDof1 && f.value(1) == "1";
        const long long id = f.value(0).toLongLong();
        onPulled = onPulled && pulled
                   && std::find(pulled->members.begin(), pulled->members.end(), id - 1) != pulled->members.end();
        noCorner = noCorner && !corners.count(id);
        sum += f.value(2).toDouble();
    }
    CHECK(onDof1 && onPulled && noCorner);
    CHECK(std::abs(sum - 126000) < 1e-9 * 126000);
}

CASE(a_pressure_is_written_on_each_element_face_of_the_group)
{
    QString error;
    const QString deck = deckWithLoad(R"({"group": "Pulled", "pressure": -210})", &error);
    CHECK(error.isEmpty());
    const QStringList lines = block(deck, "*DLOAD");
    CHECK(!lines.isEmpty());
    bool shaped = true;
    for (const QString &line : lines) {
        const QStringList f = line.split(", ");
        shaped = shaped && f.size() == 3 && QRegularExpression("^P[1-4]$").match(f[1]).hasMatch()
                 && f[2] == "-210";
    }
    CHECK(shaped);
}

CASE(gravity_is_written_with_its_size_and_direction)
{
    QString error;
    const QString deck = deckWithLoad(R"({"gravity": {"z": -9810}})", &error, "7.9e-9");
    CHECK(error.isEmpty());
    CHECK(block(deck, "*DLOAD") == QStringList({"EALL, GRAV, 9810, 0, 0, -1"}));
    const QString onGroup = deckWithLoad(R"({"group": "Steel part", "gravity": {"x": 3, "y": 4}})", &error, "7.9e-9");
    CHECK(block(onGroup, "*DLOAD") == QStringList({"Steel_part, GRAV, 5, 0.6, 0.8, 0"}));
}

CASE(gravity_on_a_material_without_density_is_refused)
{
    QString error;
    deckWithLoad(R"({"gravity": {"z": -9810}})", &error);
    CHECK(error == "step Pull: gravity, but material Steel has no Density");
    deckWithLoad(R"({"gravity": {"z": 0}})", &error, "7.9e-9");
    CHECK(error == "step Pull: gravity of size 0");
}

CASE(a_load_on_a_face_inside_the_mesh_is_refused)
{
    // The two boxes, joined, share their contact face (x = 10): it has no
    // outside for a pressure or a force to act on.
    ResultsSeries m = mesh("twoboxes.step");
    int contact = 0;
    for (int f = 1; contact == 0 && setOf(m, QString("Face%1").arg(f), true); ++f) {
        bool on = true;
        for (vtkIdType p : setOf(m, QString("Face%1").arg(f), true)->members) {
            double x[3];
            m.points.front().grid->GetPoint(p, x);
            on = on && std::abs(x[0] - 10) < 1e-9;
        }
        contact = on ? f : 0;
    }
    CHECK(contact > 0);
    QJsonObject o = json(example);
    o["groups"] = QJsonObject{{"Both", QJsonObject{{"solids", QJsonArray{1, 2}}}},
                              {"Contact", QJsonObject{{"faces", QJsonArray{contact}}}}};
    o["assign"] = QJsonObject{{"Both", "Steel"}};
    o["supports"] = QJsonArray{};
    for (const char *load : {R"({"group": "Contact", "pressure": 5})", R"({"group": "Contact", "force": {"x": 1}})"}) {
        QJsonObject step{{"name", "Push"}, {"kind", "static"},
                         {"loads", QJsonArray{QJsonDocument::fromJson(QByteArray("[") + load + "]").array().at(0)}}};
        o["steps"] = QJsonArray{step};
        const Setup s = setup(o);
        ResultsSeries mm = m;
        QString error;
        CHECK(gf::mesh::addGroupSets(mm, s, &error));
        analysisInp(s, mm, "mesh.inp", &error);
        CHECK(error == "step Push: group \"Contact\" has no face on the outside of the mesh for the load to act on");
    }
}

CASE(increments_are_written_on_the_static_line)
{
    QString error;
    const QString deck = deckWith([](QJsonObject &o) {
        QJsonArray steps = o["steps"].toArray();
        QJsonObject st = steps[0].toObject();
        st["increments"] = QJsonObject{{"initial", 0.02}, {"period", 1}, {"min", 1e-5}, {"max", 0.05}};
        steps[0] = st;
        o["steps"] = steps;
    }, &error);
    const QStringList b = body(deck);
    // ccx stops a step at 100 increments unless told (INC); room for the
    // smallest increment over the whole period.
    CHECK(b.contains("*STEP, INC=100000") && b.contains("*STATIC"));
    const int at = b.indexOf("*STATIC");
    CHECK(at > 0 && b.value(at + 1) == "0.02, 1, 1e-05, 0.05");
    QString none;
    CHECK(!body(deckWith([](QJsonObject &) {}, &none)).join('\n').contains("INC="));
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
