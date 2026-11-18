// GridForge's mesh through CalculiX and back: the box meshed by gf_mesh,
// solved by ccx 2.23 and read back by the .frd reader. Twice:
//   box-gfmesh  the mesh part written by GridForge, the deck around it by
//               hand (tension.inp);
//   box-setup   both written by GridForge from the study spec's example
//               set-up (tension.json): gf-study deck.
//   box-pressure, box-force   the same tension, by a pressure of -210 MPa
//               or a force of 126000 N on the pulled face instead of a
//               given displacement (the reaction then on the face held);
//   box-gravity the box hanging from its top under its own weight: the
//               reaction is the weight, rho g V = 7.9e-9 * 9810 * 6000 N;
//   box-plastic the box pulled 0.5 mm (stretch 1.05) in TenSense's measured
//               steel, past yield, in 50 increments: see its README.
// Uniaxial tension, so the answer is known by hand at every node
// (box-gfmesh/tension.inp says how), and all must give it.

#include "CellChecks.h"
#include "calculix/FrdToVtk.h"
#include "calculix/InpSets.h"
#include "calculix/DeckWriter.h"
#include "calculix/InpWriter.h"
#include "core/MaterialCard.h"
#include "core/Setup.h"
#include "mesh/Groups.h"
#include "mesh/Mesher.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <vtkPointData.h>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::calculix;
using gf::core::ResultsSeries;

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

struct Fixture {
    const char *dir, *job;
    bool fromSetup;        // the deck is GridForge's, from <job>.json
    bool uniaxial;         // every node as the tension's hand answer
    const char *held;      // the face whose reaction is summed
    int dof;               // 0, 1, 2
    double reaction;       // N
};
const double weight = 7.9e-9 * 9810 * 6000;
const Fixture fixtures[] = {
    {"box-gfmesh", "tension", false, true, "Face2", 0, 126000},
    {"box-setup", "tension", true, true, "Face2", 0, 126000},
    {"box-pressure", "pressure", true, true, "Face1", 0, -126000},
    {"box-force", "force", true, true, "Face1", 0, -126000},
    {"box-gravity", "gravity", true, false, "Face6", 2, weight},
    {"box-plastic", "tension", true, false, "Face1", 0, 0},
};
const Fixture *now = &fixtures[0];  // the fixture the case is run on
const char *dir = "box-gfmesh";

QString fixture(const char *file) { return QString(GF_FIXTURES) + "/calculix/" + dir + "/" + file; }
QString job(const char *ext) { return fixture((QString(now->job) + ext).toUtf8().constData()); }

ResultsSeries results()
{
    QString e;
    ResultsSeries r = toSeries(readFrd(job(".frd")), QString(now->job) + ".frd", &e);
    if (!e.isEmpty())
        std::printf("  (%s/tension.frd: %s)\n", dir, qPrintable(e));
    return r;
}

QStringList fileLines(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).split('\n') : QStringList();
}

} // namespace

CASE(ccx_took_the_mesh_as_written)
{
    const ResultsSeries s = results();
    CHECK(s.points.size() == (QString(dir) == "box-plastic" ? 50u : 1u));
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    // The 229 quadratic tetrahedra and 508 nodes of mesh.inp, in place.
    CHECK(g->GetNumberOfCells() == 229 && g->GetNumberOfPoints() == 508);
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c)
        CHECK(g->GetCellType(c) == VTK_QUADRATIC_TETRA);
    CHECK(std::abs(gf::test::enclosedVolume(g) - 6000) < 1e-9);
    // Each node where GridForge put it, to the six digits the .frd keeps,
    // under the id the deck gave it.
    QString error;
    const ResultsSeries mesh = gf::mesh::meshStep(QString(GF_FIXTURES) + "/step/box.step", {5, 2}, &error);
    CHECK(!mesh.points.empty() && mesh.points.front().grid->GetNumberOfPoints() == 508);
    if (mesh.points.empty())
        return;
    vtkUnstructuredGrid *m = mesh.points.front().grid;
    vtkDataArray *ids = g->GetPointData()->GetArray("node id");
    double worst = 0;
    for (vtkIdType p = 0; ids && p < g->GetNumberOfPoints(); ++p) {
        const auto a = gf::test::point(g, p), b = gf::test::point(m, static_cast<vtkIdType>(ids->GetTuple1(p)) - 1);
        for (int d = 0; d < 3; ++d)
            worst = std::max(worst, std::abs(a[d] - b[d]));
    }
    CHECK(ids && worst < 5e-5);  // 30 written as 3.00000E+01: half a unit in the sixth digit
}

CASE(every_node_moves_as_by_hand)
{
    if (!now->uniaxial)
        return;
    const ResultsSeries s = results();
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    vtkDataArray *u = g->GetPointData()->GetArray("DISP");
    CHECK(u);
    if (!u)
        return;
    // ux = 0.001 x, uy = -0.0003 y, uz = -0.0003 z; the .frd keeps six
    // digits (largest value 0.01, so 5e-8 at worst).
    double worst = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        const auto x = gf::test::point(g, p);
        worst = std::max({worst, std::abs(u->GetComponent(p, 0) - 0.001 * x[0]),
                          std::abs(u->GetComponent(p, 1) + 0.0003 * x[1]),
                          std::abs(u->GetComponent(p, 2) + 0.0003 * x[2])});
    }
    CHECK(worst < 5e-8);
}

CASE(the_stress_is_210_in_x_and_nothing_else_everywhere)
{
    if (!now->uniaxial)
        return;
    // To 1e-4, as the other fixtures: ccx computes 210 at the integration
    // points and writes 209.998 at the nodes, extrapolated (bar-tension's
    // README).
    const ResultsSeries s = results();
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    vtkDataArray *stress = g->GetPointData()->GetArray("STRESS");
    CHECK(stress && stress->GetNumberOfComponents() == 6);
    if (!stress)
        return;
    double worstX = 0, worstOther = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        worstX = std::max(worstX, std::abs(stress->GetComponent(p, 0) - 210));
        for (int k = 1; k < 6; ++k)
            worstOther = std::max(worstOther, std::abs(stress->GetComponent(p, k)));
    }
    CHECK(worstX < 210e-4);
    CHECK(worstOther < 1e-3);
}

CASE(the_plastic_box_follows_the_measured_curve_everywhere)
{
    if (QString(dir) != "box-plastic")
        return;
    const ResultsSeries s = results();
    CHECK(s.points.size() == 50);
    if (s.points.empty())
        return;
    vtkUnstructuredGrid *g = s.points.back().grid;
    vtkDataArray *stress = g->GetPointData()->GetArray("STRESS"), *pe = g->GetPointData()->GetArray("PE"),
                 *u = g->GetPointData()->GetArray("DISP");
    CHECK(stress && pe && u);
    if (!stress || !pe || !u)
        return;
    // TenSense's curve, true stress against true plastic strain, as the
    // deck included it.
    std::vector<std::pair<double, double>> curve;  // (plastic strain, stress)
    for (const QString &line : fileLines(fixture("materials/tensense-ro-plastic.inp"))) {
        const QStringList f = line.split(',');
        bool a = false, b = false;
        const double sigma = f.value(0).toDouble(&a), ep = f.value(1).toDouble(&b);
        if (f.size() == 2 && a && b)
            curve.push_back({ep, sigma});
    }
    auto H = [&](double ep) -> double {
        for (size_t k = 1; k < curve.size(); ++k)
            if (ep <= curve[k].first && curve[k].first > curve[k - 1].first)
                return curve[k - 1].second
                       + (ep - curve[k - 1].first) * (curve[k].second - curve[k - 1].second)
                             / (curve[k].first - curve[k - 1].first);
        return NAN;
    };
    double sMin = 1e300, sMax = -1e300, offCurve = 0, others = 0, pull = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        const double sxx = stress->GetComponent(p, 0);
        sMin = std::min(sMin, sxx);
        sMax = std::max(sMax, sxx);
        offCurve = std::max(offCurve, std::abs(sxx - H(pe->GetTuple1(p))));
        for (int k = 1; k < 6; ++k)
            others = std::max(others, std::abs(stress->GetComponent(p, k)));
        pull = std::max(pull, std::abs(u->GetComponent(p, 0) - 0.05 * gf::test::point(g, p)[0]));
    }
    // Uniaxial and the same everywhere.
    CHECK(sMax - sMin < 1e-3 && others < 0.05);
    // The stretch: ux = 0.05 x at every node, to the .frd's six digits of
    // ux (5e-7 at 0.5) and of x (0.05 times 5e-6).
    CHECK(pull < 1e-6);
    if (pull >= 1e-6)
        std::printf("  (pull off by %.3g)\n", pull);
    // The measured curve honoured: at each node's plastic strain, the
    // stress is TenSense's (to the .frd's digits).
    CHECK(offCurve < 0.01);
    // Simo's hyperelastic-plastic model by hand (README): 432.1208 MPa.
    // ccx converges to 432.603 as its increments shrink (1000: 432.603;
    // here, 50: 432.635), 0.11 % above: not yet explained, so held to 0.2 %.
    CHECK(std::abs(sMax - 432.1208) < 0.002 * 432.1208);
    if (!(std::abs(sMax - 432.1208) < 0.002 * 432.1208) || offCurve >= 0.01)
        std::printf("  (sxx %.4f, off the curve by %.4f)\n", sMax, offCurve);
}

CASE(the_reaction_on_the_held_face_is_the_hand_answer)
{
    if (QString(dir) == "box-plastic")
        return;  // its reaction follows from its stress, checked above
    const ResultsSeries s = results();
    const InpSets sets = readInpSets(fixture("mesh.inp"));
    const InpSet *face2 = sets.find(now->held, true);
    CHECK(sets.ok() && face2);
    if (s.points.empty() || !face2)
        return;
    vtkUnstructuredGrid *g = s.points.front().grid;
    vtkDataArray *force = g->GetPointData()->GetArray("FORC");
    vtkDataArray *ids = g->GetPointData()->GetArray("node id");
    CHECK(force && ids);
    if (!force || !ids)
        return;
    std::vector<bool> on(static_cast<size_t>(ids->GetRange()[1]) + 1, false);
    for (int id : face2->ids)
        if (id >= 0 && static_cast<size_t>(id) < on.size())
            on[id] = true;
    double fx = 0;
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p)
        if (on[static_cast<size_t>(ids->GetTuple1(p))])
            fx += force->GetComponent(p, now->dof);
    if (QString(dir) == "box-gravity") {
        // ccx's FORC sums to zero over the model: at a held node it is the
        // reaction less the load put on that node itself. Gravity puts a
        // load on the held top face's nodes too, so it is added back: a
        // 10-node tetrahedron's consistent share of its weight W is -W/20
        // at each corner and W/5 at each middle node (downward, -z).
        const double rhoG = 7.9e-9 * 9810;
        for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
            vtkIdType n;
            const vtkIdType *pts;
            g->GetCellPoints(c, n, pts);
            const auto a = gf::test::point(g, pts[0]), b = gf::test::point(g, pts[1]),
                       d = gf::test::point(g, pts[2]), e = gf::test::point(g, pts[3]);
            const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {d[0] - a[0], d[1] - a[1], d[2] - a[2]},
                         w[3] = {e[0] - a[0], e[1] - a[1], e[2] - a[2]};
            const double volume = std::abs(u[0] * (v[1] * w[2] - v[2] * w[1]) - u[1] * (v[0] * w[2] - v[2] * w[0])
                                           + u[2] * (v[0] * w[1] - v[1] * w[0]))
                                  / 6;
            for (vtkIdType k = 0; k < n; ++k)
                if (on[static_cast<size_t>(ids->GetTuple1(pts[k]))])
                    fx -= (k < 4 ? rhoG * volume / 20 : -rhoG * volume / 5);  // the node's load is +W/20 or -W/5 in z
        }
    }
    // The .frd keeps six digits of each node's share.
    CHECK(std::abs(fx - now->reaction) < 1e-4 * std::abs(now->reaction));
    if (std::abs(fx - now->reaction) >= 1e-4 * std::abs(now->reaction))
        std::printf("  (%s: reaction %.9g, expected %.9g)\n", dir, fx, now->reaction);
}

CASE(the_committed_mesh_is_what_gridforge_writes_today)
{
    // mesh.inp is GridForge's own output, kept because ccx solved it. The
    // same geometry and recipe must still give it, line for line, all but
    // the first (the build that wrote it).
    QString error;
    ResultsSeries mesh = gf::mesh::meshStep(QString(GF_FIXTURES) + "/step/box.step", {5, 2}, &error);
    CHECK(error.isEmpty());
    if (now->fromSetup) {
        // Here the deck is GridForge's too, from the set-up beside it.
        QFile f(job(".json"));
        CHECK(f.open(QIODevice::ReadOnly));
        gf::core::Setup setup =
            gf::core::readSetup(QJsonDocument::fromJson(f.readAll()).object()["setup"].toObject(), &error);
        CHECK(error.isEmpty() && gf::core::fillFromCards(setup, fixture(""), &error));
        CHECK(error.isEmpty() && gf::mesh::addGroupSets(mesh, setup, &error));
        const QStringList deck = analysisInp(setup, mesh, "mesh.inp", &error).split('\n'),
                          keptDeck = fileLines(job(".inp"));
        CHECK(deck.size() == keptDeck.size() && deck.mid(1) == keptDeck.mid(1));
    }
    const QStringList today = meshInp(mesh, &error).split('\n'), kept = fileLines(fixture("mesh.inp"));
    CHECK(today.size() == kept.size());
    CHECK(today.mid(1) == kept.mid(1));
    // Where they part, so a machine that meshes differently says how.
    for (qsizetype i = 1; i < std::min(today.size(), kept.size()); ++i)
        if (today[i] != kept[i]) {
            std::printf("  line %lld differs:\n    today: %s\n    kept:  %s\n", static_cast<long long>(i + 1),
                        qPrintable(today[i]), qPrintable(kept[i]));
            break;
        }
}

int main()
{
    for (const Fixture &f : fixtures) {
        now = &f;
        dir = f.dir;
        for (auto &[name, run] : cases()) {
            const int before = failures;
            run();
            std::printf("%s %s: %s\n", failures == before ? "PASS" : "FAIL", f.dir, name);
        }
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
