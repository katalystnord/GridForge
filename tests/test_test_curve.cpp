// Measured against predicted (core/TestCurve): TenSense's analysed curve
// read as it wrote it, a results file's curve through a virtual
// extensometer and a group's reactions, and the two compared. The answers
// come from outside the code under test: the .vtp's text read with Python
// (fixtures/tensense/README.md), and box-plastic's own fields (the true
// stress over the deformed section, the displacements at the gauge
// points), read through the frd reader the other tests hold.

#include "calculix/FrdReader.h"
#include "calculix/FrdToVtk.h"
#include "calculix/InpSets.h"
#include "core/ResultsSeries.h"
#include "core/TestCurve.h"

#include <QTemporaryDir>

#include <vtkDataArray.h>
#include <vtkPointData.h>

#include <cmath>
#include <cstdio>

using namespace gf::core;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
#endif

namespace {
int failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)
bool near(double a, double b, double rel) { return std::abs(a - b) <= rel * std::max(std::abs(a), std::abs(b)); }
} // namespace

int main()
{
    const QString fixtures = GF_FIXTURES;
    QString error;

    // TenSense's curve of BAM's Zx3, as Python read it.
    const TestCurve zx3 = readTenSenseCurve(fixtures + "/tensense/Zx3.vtp", &error);
    CHECK(error.isEmpty());
    CHECK(zx3.strain.size() == 1906 && zx3.force.size() == 1906);
    if (zx3.strain.size() == 1906) {
        CHECK(zx3.strain.front() == -0.0002301 && zx3.strain.back() == 0.0390524);
        CHECK(near(zx3.force.front(), 205.36, 1e-12) && near(zx3.force.back(), 53164.14, 1e-12));
    }
    CHECK(zx3.source.contains("Zx3.vtp"));
    readTenSenseCurve(fixtures + "/step/box.step", &error);
    CHECK(!error.isEmpty());

    // box-plastic: the box (x 0 to 10) pulled 0.5 mm along x over 50
    // increments. An extensometer from x = 2 to x = 8 on its axis, the
    // force on Pulled (x = 10).
    const QString job = fixtures + "/calculix/box-plastic/tension";
    ResultsSeries series = gf::calculix::toSeries(gf::calculix::readFrd(job + ".frd"), "tension.frd", &error);
    gf::calculix::addSets(series, gf::calculix::readInpSets(job + ".inp"), "tension.inp");
    QTemporaryDir dir;
    const QString results = dir.filePath("results.vtkhdf");
    CHECK(writeVtkHdf(series, results, &error));
    const std::array<double, 3> a{2, 10, 15}, b{8, 10, 15};
    const TestCurve box = predictedCurve(results, "Pulled", a, b, &error);
    CHECK(error.isEmpty());
    CHECK(box.strain.size() == series.points.size() + 1);
    if (box.strain.size() == series.points.size() + 1) {
        CHECK(box.strain[0] == 0 && box.force[0] == 0);
        for (size_t k = 0; k < series.points.size(); ++k) {
            vtkUnstructuredGrid *g = series.points[k].grid;
            vtkDataArray *u = g->GetPointData()->GetArray("DISP");
            vtkDataArray *s = g->GetPointData()->GetArray("STRESS");
            // The answer from the fields themselves: the stretch is uniform
            // (ux = 0.05 x at the end), so the gauge's strain is the end's
            // ux over 10; the force is the true sxx times the deformed
            // section, (20 + uy at y = 20) by (30 + uz at z = 30).
            double ux = 0, uy = 0, uz = 0, sxx = 0;
            for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
                double x[3];
                g->GetPoint(p, x);
                if (x[0] == 10)
                    ux = u->GetComponent(p, 0);
                if (x[1] == 20)
                    uy = u->GetComponent(p, 1);
                if (x[2] == 30)
                    uz = u->GetComponent(p, 2);
            }
            sxx = s->GetComponent(0, 0);
            const double strain = ux / 10, force = sxx * (20 + uy) * (30 + uz);
            if (!near(box.strain[k + 1], strain, 1e-5) || !near(box.force[k + 1], force, 2e-4)) {
                std::printf("  increment %zu: strain %.8g (%.8g), force %.8g (%.8g)\n", k + 1, box.strain[k + 1],
                            strain, box.force[k + 1], force);
                CHECK(false);
                break;
            }
        }
        CHECK(near(box.strain.back(), 0.05, 1e-5));
    }
    // Faults said: a set not there, a gauge point outside the part.
    predictedCurve(results, "Nothing", a, b, &error);
    CHECK(error == "no node set \"Nothing\" in the results");
    predictedCurve(results, "Pulled", {50, 10, 15}, b, &error);
    CHECK(error == "the extensometer's point (50, 10, 15) is outside the part");

    // The comparison: measured F = 1000 e; predicted 1 % over at 0.01, 2 %
    // under at 0.02, and one point beyond the measured strains.
    const TestCurve measured{{0, 0.005, 0.015, 0.03}, {0, 5, 15, 30}, "m"};
    const TestCurve predicted{{0, 0.01, 0.02, 0.05}, {0, 10.1, 19.6, 50}, "p"};
    const CurveComparison c = compareCurves(measured, predicted);
    CHECK(c.points.size() == 2 && c.outside == 1);
    if (c.points.size() == 2) {
        CHECK(near(c.points[0].measured, 10, 1e-12) && near(c.points[0].difference(), 0.01, 1e-9));
        CHECK(near(c.points[1].measured, 20, 1e-12) && near(c.points[1].difference(), -0.02, 1e-9));
    }
    CHECK(comparisonWords(c) == "predicted against measured, at 2 strains from 1 % to 2 %: at most 2 % apart "
                                "(under, at 2 %), 1.5 % on average; 1 predicted point beyond the measured "
                                "strains, not compared");
    // The first time the measured strain reaches a value: a curve that
    // goes back (unloading) is not read twice.
    // At 0.015: 15 on the way up, not 11.25 on the way back.
    const TestCurve back{{0, 0.02, 0.01, 0.03}, {0, 20, 5, 30}, "m"};
    const CurveComparison once = compareCurves(back, TestCurve{{0.015}, {15}, "p"});
    CHECK(once.points.size() == 1 && near(once.points[0].measured, 15, 1e-12));
    // A measured curve that starts above the strain asked is not
    // extrapolated back to it.
    const CurveComparison above = compareCurves(TestCurve{{0.02, 0.03}, {20, 30}, "m"}, TestCurve{{0.01}, {10}, "p"});
    CHECK(above.points.empty() && above.outside == 1);

    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
