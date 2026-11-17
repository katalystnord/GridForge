// A run's results as the window reads them, from a .vtkhdf the core wrote
// from ccx's bar-increments results: four increments, the tip moving
// 0.025 mm in z each (fixtures/calculix/bar-increments/README.md).

#include "calculix/FrdToVtk.h"
#include "calculix/InpSets.h"
#include "gui/ResultsFile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <vtkDataArray.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using gf::gui::ResultsFile;

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

// bar-increments' results written as the toolkit writes them.
QString barResults(QTemporaryDir &dir)
{
    const QString frd = QString(GF_FIXTURES) + "/calculix/bar-increments/bar.frd";
    QString error;
    gf::core::ResultsSeries s = gf::calculix::toSeries(gf::calculix::readFrd(frd), "bar.frd", &error);
    gf::calculix::addSets(s, gf::calculix::readInpSets(QString(GF_FIXTURES) + "/calculix/bar-increments/bar.inp"),
                          "bar.inp");
    const QString path = QDir(dir.path()).filePath("results.vtkhdf");
    gf::core::writeVtkHdf(s, path, &error);
    return path;
}

// The value of "shown" at ccx's node `id`.
double shownAt(vtkUnstructuredGrid *g, int id)
{
    vtkDataArray *ids = g->GetPointData()->GetArray("node id"), *shown = g->GetPointData()->GetArray("shown");
    for (vtkIdType p = 0; ids && shown && p < g->GetNumberOfPoints(); ++p)
        if (static_cast<int>(ids->GetTuple1(p)) == id)
            return shown->GetTuple1(p);
    return NAN;
}

} // namespace

CASE(the_increments_and_fields_are_read)
{
    QTemporaryDir dir;
    ResultsFile r;
    QString error;
    CHECK(r.open(barResults(dir), &error) && error.isEmpty());
    CHECK(r.timeCount() == 4);
    for (int k = 0; k < r.timeCount() && k < 4; ++k)
        CHECK(r.time(k) == 0.25 * (k + 1));
    const QStringList f = r.fields();
    CHECK(f.contains("DISP (magnitude)") && f.contains("von Mises stress (derived)"));
    CHECK(!f.contains("STRESS") && !f.contains("node id"));
    CHECK(r.preferredField() == "von Mises stress (derived)");  // no plastic strain here
}

CASE(the_model_at_each_increment_carries_the_field_asked_for)
{
    QTemporaryDir dir;
    ResultsFile r;
    QString error;
    r.open(barResults(dir), &error);
    for (int k = 0; k < 4; ++k) {
        vtkSmartPointer<vtkUnstructuredGrid> g = r.model(k, "DISP (magnitude)");
        CHECK(g && g->GetNumberOfPoints() == 20 && g->GetNumberOfCells() == 4);
        if (!g)
            continue;
        // The tip's z is 0.025 (k + 1); x and y contract a little, so the
        // magnitude is a hair above it.
        const double m = shownAt(g, 20);
        CHECK(m >= 0.025 * (k + 1) && m < 0.025 * (k + 1) * 1.001);
    }
    const auto [lo, hi] = r.range(3, "DISP (magnitude)");
    CHECK(lo == 0 && hi >= 0.1 && hi < 0.1001);
}

CASE(a_single_time_point_keeps_its_time)
{
    // box-setup's tension: one increment at time 1 (ccx's), which VTK's
    // reader does not list as a time step.
    QTemporaryDir dir;
    QString error;
    gf::core::ResultsSeries s = gf::calculix::toSeries(
        gf::calculix::readFrd(QString(GF_FIXTURES) + "/calculix/box-setup/tension.frd"), "tension.frd", &error);
    const QString path = QDir(dir.path()).filePath("one.vtkhdf");
    gf::core::writeVtkHdf(s, path, &error);
    {
        ResultsFile r;
        CHECK(r.open(path, &error));
        CHECK(r.timeCount() == 1 && r.time(0) == 0.0);  // nothing says otherwise
    }
    // The toolkit's results.json beside it lists the time points.
    QFile report(QDir(dir.path()).filePath("results.json"));
    report.open(QIODevice::WriteOnly);
    report.write(R"({"format": "gridforge-results", "time points": [1]})");
    report.close();
    ResultsFile r;
    CHECK(r.open(path, &error));
    CHECK(r.timeCount() == 1 && r.time(0) == 1.0);
}

CASE(a_file_that_cannot_be_read_is_said)
{
    ResultsFile r;
    QString error;
    CHECK(!r.open("/no/such/results.vtkhdf", &error));
    CHECK(error == "cannot read /no/such/results.vtkhdf");
}

// A node's place in a model.
static std::array<double, 3> pointAt(vtkUnstructuredGrid *g, int id)
{
    vtkDataArray *ids = g->GetPointData()->GetArray("node id");
    for (vtkIdType p = 0; ids && p < g->GetNumberOfPoints(); ++p)
        if (static_cast<int>(ids->GetTuple1(p)) == id) {
            std::array<double, 3> x;
            g->GetPoint(p, x.data());
            return x;
        }
    return {NAN, NAN, NAN};
}

CASE(the_model_is_deformed_by_its_displacements_times_a_factor)
{
    QTemporaryDir dir;
    ResultsFile r;
    QString error;
    r.open(barResults(dir), &error);
    CHECK(r.canDeform());
    const auto at = pointAt(r.model(3, "DISP (magnitude)"), 20);  // undeformed by default
    // The tip moves 0.1 in z at the last increment (the bar's known answer).
    const auto once = pointAt(r.model(3, "DISP (magnitude)", 1.0), 20);
    const auto tenfold = pointAt(r.model(3, "DISP (magnitude)", 10.0), 20);
    CHECK(std::abs(once[2] - at[2] - 0.1) < 1e-6);
    CHECK(std::abs(tenfold[2] - at[2] - 1.0) < 1e-5);
    // The colour is of the field, not moved by the factor.
    CHECK(shownAt(r.model(3, "DISP (magnitude)", 10.0), 20) == shownAt(r.model(3, "DISP (magnitude)"), 20));
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
