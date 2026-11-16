// The .frd reader against results written by a released ccx, each with an
// answer known by hand (tests/fixtures/calculix/*/README.md). The reader
// must give back exactly the numbers the file holds; the physics checks
// say those numbers are the right ones.

#include "calculix/FrdReader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::calculix;

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

QString fixture(const char *rel) { return QString(GF_FIXTURES) + "/calculix/" + rel; }

const FrdFile &bar()
{
    static const FrdFile f = readFrd(fixture("bar-tension/bar.frd"));
    return f;
}

const FrdFile &cantilever()
{
    static const FrdFile f = readFrd(fixture("cantilever-te10/cantilever.frd"));
    return f;
}

const FrdFile &increments()
{
    static const FrdFile f = readFrd(fixture("bar-increments/bar.frd"));
    return f;
}

const FrdFile &binary()
{
    static const FrdFile f = readFrd(fixture("bar-binary/bar.frd"));
    return f;
}

bool near(double a, double b, double rel) { return std::abs(a - b) <= rel * std::abs(b); }

double at(const FrdDataset *d, int node, const char *component)
{
    double v = NAN;
    if (!d || !d->valueAt(node, component, &v))
        return NAN;
    return v;
}

} // namespace

CASE(a_bar_in_tension_reads_without_error)
{
    CHECK(bar().ok());
    if (!bar().ok())
        std::printf("  (%s)\n", qPrintable(bar().error));
}

CASE(the_bar_has_twenty_nodes_and_four_bricks)
{
    const FrdFile &f = bar();
    CHECK(f.nodes.size() == 20);
    const FrdNode *n = f.node(20);
    CHECK(n && n->x == 0.0 && n->y == 10.0 && n->z == 100.0);
    CHECK(f.elements.size() == 4);
    if (f.elements.size() == 4) {
        const FrdElement &e = f.elements[3];
        CHECK(e.id == 4 && e.type == FrdElementType::He8 && e.material == 1);
        CHECK((e.nodes == std::vector<int>{13, 14, 15, 16, 17, 18, 19, 20}));
    }
}

CASE(the_bar_says_it_was_written_by_ccx_2_23)
{
    bool found = false;
    for (const QString &line : bar().userHeader)
        if (line.startsWith("VERSION") && line.contains("Version 2.23"))
            found = true;
    CHECK(found);
}

CASE(the_bar_holds_five_datasets_at_the_end_of_step_one)
{
    const FrdFile &f = bar();
    const QStringList expected = {"DISP", "STRESS", "TOSTRAIN", "FORC", "ERROR"};
    CHECK(int(f.datasets.size()) == expected.size());
    for (int i = 0; i < int(f.datasets.size()) && i < expected.size(); ++i) {
        const FrdDataset &d = f.datasets[i];
        CHECK(d.name == expected[i]);
        CHECK(d.step == 1 && d.value == 1.0 && d.analysisType == 0);
        CHECK(d.nodeIds.size() == 20);
    }
    const FrdDataset *disp = f.dataset("DISP");
    CHECK(disp && disp->components.size() == 4 && disp->suppliedCount() == 3);
    CHECK(disp && disp->components.size() == 4 && disp->components[3].name == "ALL"
          && !disp->components[3].supplied);
    CHECK(disp && disp->parameters.size() == 1 && disp->parameters.value(0).startsWith("STEP"));
}

// The far end moved 0.1 mm; the bar contracts by nu * strain * width.
// Node 20's line is "0.00000E+00-3.00000E-03 1.00000E-01": fixed columns.
CASE(the_bar_reads_back_its_displacement)
{
    const FrdDataset *d = bar().dataset("DISP");
    CHECK(at(d, 20, "D1") == 0.0);
    CHECK(at(d, 20, "D2") == -3.0e-3);
    CHECK(at(d, 20, "D3") == 1.0e-1);
    for (const FrdNode &n : bar().nodes)
        CHECK(std::abs(at(d, n.id, "D3") - 1.0e-3 * n.z) < 1e-9);
}

// Exact in the file is 209.992 (ccx extrapolates from integration points,
// where it is 210.000); within 1e-4 of the hand answer, E * strain.
CASE(the_bar_reads_back_its_stress_and_strain)
{
    const FrdDataset *s = bar().dataset("STRESS");
    const FrdDataset *e = bar().dataset("TOSTRAIN");
    CHECK(at(s, 1, "SZZ") == 2.09992e+02);
    CHECK(at(e, 20, "EZZ") == 9.99960e-04);
    CHECK(at(e, 20, "EXX") == -2.99988e-04);
    for (const FrdNode &n : bar().nodes) {
        CHECK(near(at(s, n.id, "SZZ"), 210.0, 1e-4));
        CHECK(std::abs(at(s, n.id, "SXX")) < 1e-9);
        CHECK(near(at(e, n.id, "EZZ"), 1.0e-3, 1e-4));
        CHECK(near(at(e, n.id, "EYY"), -3.0e-4, 1e-4));
    }
}

// 210 MPa over 100 mm2: 21000 N on the moved end, 5250 N on each corner.
CASE(the_reaction_on_the_moved_end_is_21000_newton)
{
    const FrdDataset *f = bar().dataset("FORC");
    double sum = 0;
    for (int node : {17, 18, 19, 20})
        sum += at(f, node, "F3");
    CHECK(at(f, 17, "F3") == 5.25e+03);
    CHECK(near(sum, 21000.0, 1e-9));
}

// Quadratic tetrahedra: ccx writes C3D10 as type 6, nodes in the deck's
// order (cantilever-te10/README.md has the beam theory).
CASE(a_cantilever_in_te10_reads_as_480_ten_node_tetrahedra)
{
    const FrdFile &f = cantilever();
    CHECK(f.ok());
    if (!f.ok())
        std::printf("  (%s)\n", qPrintable(f.error));
    CHECK(f.nodes.size() == 1025);
    CHECK(f.elements.size() == 480);
    if (!f.elements.empty()) {
        const FrdElement &e = f.elements[0];
        CHECK(e.type == FrdElementType::Te10);
        CHECK((e.nodes == std::vector<int>{1, 2, 5, 14, 190, 191, 192, 193, 194, 195}));
    }
    for (const FrdElement &e : f.elements)
        CHECK(e.type == FrdElementType::Te10 && e.nodes.size() == 10);
}

// Timoshenko: P L^3 / 3 E I + P L / (5/6 G A) = 0.19196 mm; the mesh
// gives 0.18983, 1.1 % under it.
CASE(the_cantilever_tip_deflects_as_beam_theory_says)
{
    const FrdFile &f = cantilever();
    const FrdDataset *d = f.dataset("DISP");
    double sum = 0;
    int count = 0;
    for (const FrdNode &n : f.nodes)
        if (n.z == 100.0) {
            sum += at(d, n.id, "D2");
            ++count;
        }
    CHECK(count == 25);
    CHECK(near(sum / count, 0.19196, 0.02));
}

// M c / I at mid-span: 5000 N mm * 5 mm / 833.33 mm4 = 30 MPa, tension
// on the side y = 0 (the load is in +y); the reaction is the load.
CASE(the_cantilever_bends_and_reacts_as_beam_theory_says)
{
    const FrdFile &f = cantilever();
    const FrdDataset *s = f.dataset("STRESS");
    const FrdDataset *r = f.dataset("FORC");
    for (const FrdNode &n : f.nodes) {
        if (n.z == 50.0 && n.x == 5.0 && n.y == 0.0)
            CHECK(near(at(s, n.id, "SZZ"), 30.0, 0.015));
        if (n.z == 50.0 && n.x == 5.0 && n.y == 10.0)
            CHECK(near(at(s, n.id, "SZZ"), -30.0, 0.015));
    }
    double reaction = 0;
    for (const FrdNode &n : f.nodes)
        if (n.z == 0.0)
            reaction += at(r, n.id, "F2");
    CHECK(near(reaction, -100.0, 1e-5));
}

// Four fixed increments: each writes all five datasets; the 100C step
// field counts increments and 1PSTEP holds (running count, increment, step).
CASE(each_increment_of_the_bar_is_its_own_set_of_datasets)
{
    const FrdFile &f = increments();
    CHECK(f.ok());
    if (!f.ok())
        std::printf("  (%s)\n", qPrintable(f.error));
    CHECK(f.datasets.size() == 20);
    for (int n = 0; n < 4; ++n) {
        const FrdDataset *d = f.dataset("DISP", n);
        CHECK(d && d->step == n + 1 && d->value == 0.25 * (n + 1));
    }
    const FrdDataset *second = f.dataset("DISP", 1);
    CHECK(second && second->parameters.size() == 1
          && second->parameters.value(0).simplified() == "STEP 6 2 1");
    CHECK(f.dataset("DISP", 4) == nullptr);
}

// The end is moved: 0.025 mm a increment, exactly; the reaction grows
// with it, 5250 N a increment within NLGEOM's 0.2 %.
CASE(the_bar_moves_and_reacts_increment_by_increment)
{
    const FrdFile &f = increments();
    const double tip[] = {0.025, 0.05, 0.075, 0.1};
    for (int n = 0; n < 4; ++n) {
        CHECK(at(f.dataset("DISP", n), 20, "D3") == tip[n]);
        double sum = 0;
        for (int node : {17, 18, 19, 20})
            sum += at(f.dataset("FORC", n), node, "F3");
        CHECK(near(sum, 5250.0 * (n + 1), 2e-3));
    }
}

// Binary: the same bar as bar-tension, nodes in float64, results in
// float32 (bar-binary/README.md has the layout).
CASE(a_binary_bar_reads_as_the_same_mesh)
{
    const FrdFile &f = binary();
    CHECK(f.ok());
    if (!f.ok())
        std::printf("  (%s)\n", qPrintable(f.error));
    CHECK(f.nodes.size() == 20 && f.elements.size() == 4 && f.datasets.size() == 5);
    const FrdNode *n = f.node(20);
    CHECK(n && n->x == 0.0 && n->y == 10.0 && n->z == 100.0);
    if (f.elements.size() == 4)
        CHECK((f.elements[3].nodes == std::vector<int>{13, 14, 15, 16, 17, 18, 19, 20}));
}

CASE(a_binary_bar_holds_the_ascii_bars_numbers_unrounded)
{
    const FrdDataset *d = binary().dataset("DISP");
    CHECK(at(d, 20, "D2") == double(-3.0e-3f));
    CHECK(at(binary().dataset("STRESS"), 1, "SZZ") == double(209.99159240722656f));
    for (const QString name : {"DISP", "STRESS", "TOSTRAIN", "FORC"}) {
        const FrdDataset *a = bar().dataset(name);
        const FrdDataset *b = binary().dataset(name);
        CHECK(a && b && a->suppliedCount() == b->suppliedCount() && a->nodeIds == b->nodeIds);
        if (!a || !b || a->values.size() != b->values.size())
            continue;
        // Five digits written: within half a unit in the fifth of the
        // largest value in the dataset (the near-zero ones are noise).
        double largest = 0;
        for (double v : a->values)
            largest = std::max(largest, std::abs(v));
        for (size_t i = 0; i < a->values.size(); ++i)
            CHECK(std::abs(a->values[i] - b->values[i]) <= 5e-6 * largest);
    }
}

CASE(a_binary_file_that_ends_early_is_refused_with_its_byte)
{
    const FrdFile f = readFrd(fixture("refused/truncated-binary.frd"));
    CHECK(!f.ok());
    CHECK(f.error.contains("byte 974") && f.error.contains("6 of 20"));
    if (f.ok() || !f.error.contains("byte 974"))
        std::printf("  (%s)\n", qPrintable(f.error));
}

CASE(a_file_that_ends_early_is_refused_with_its_line)
{
    const FrdFile f = readFrd(fixture("refused/truncated.frd"));
    CHECK(!f.ok());
    CHECK(f.error.contains("line 20") && f.error.contains("ends"));
}

CASE(an_unknown_element_type_is_refused_with_its_line)
{
    const FrdFile f = readFrd(fixture("refused/unknown-element.frd"));
    CHECK(!f.ok());
    CHECK(f.error.contains("line 35") && f.error.contains("99"));
}

CASE(a_missing_file_is_refused_by_name)
{
    const FrdFile f = readFrd(fixture("no-such/job.frd"));
    CHECK(!f.ok());
    CHECK(f.error.contains("job.frd"));
}

int main()
{
    for (const auto &[name, f] : cases()) {
        const int before = failures;
        f();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%zu cases, %d checks failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
