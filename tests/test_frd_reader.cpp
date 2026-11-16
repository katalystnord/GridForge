// The .frd reader against results written by a released ccx, each with an
// answer known by hand (tests/fixtures/calculix/*/README.md). The reader
// must give back exactly the numbers the file holds; the physics checks
// say those numbers are the right ones.

#include "calculix/FrdReader.h"

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
    std::printf("%zu cases, %d failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
