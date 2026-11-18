// The mesh convergence check's arithmetic (core/Convergence), on values
// whose answer is known: f(h) = 10 + 2 h^2 is second order and 10 at
// h = 0, sampled at h = 2.25, 1.5 and 1 (r = 1.5).

#include "core/Convergence.h"
#include "core/ResultsSeries.h"

#include <QTemporaryDir>

#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

#include <cmath>
#include <cstdio>

using namespace gf::core;

namespace {
int failures = 0;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)
bool near(double a, double b) { return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b)); }
double f(double h) { return 10 + 2 * h * h; }
} // namespace

int main()
{
    // Converging: order 2, 10 at zero size; R = 2.5 / 5.625; the band
    // 1.25 * (2.5 / 12) / (1.5^2 - 1).
    const Convergence c = convergence(f(2.25), f(1.5), f(1), 1.5, 1.5);
    CHECK(c.kind == Convergence::Kind::Converging);
    CHECK(near(c.order, 2));
    CHECK(near(c.extrapolated, 10));
    CHECK(near(c.ratio, 2.5 / 5.625));
    CHECK(near(c.band, 1.25 * (2.5 / 12) / 1.25));
    // From below as well as above: -f converges to -10.
    const Convergence below = convergence(-f(2.25), -f(1.5), -f(1), 1.5, 1.5);
    CHECK(below.kind == Convergence::Kind::Converging && near(below.extrapolated, -10) && near(below.order, 2));
    // First order: 10 + h.
    const Convergence first = convergence(12.25, 11.5, 11, 1.5, 1.5);
    CHECK(first.kind == Convergence::Kind::Converging && near(first.order, 1) && near(first.extrapolated, 10));
    // Unequal steps, as meshes give them: h = 2, 1.25, 1 (ratios 1.6 and
    // 1.25), the same second order and the same 10 at h = 0; the order
    // from f = f0 + C h^p itself, the band from the finest step.
    const Convergence unequal = convergence(f(2), f(1.25), f(1), 1.6, 1.25);
    CHECK(unequal.kind == Convergence::Kind::Converging);
    CHECK(std::abs(unequal.order - 2) < 1e-9);
    CHECK(std::abs(unequal.extrapolated - 10) < 1e-9);
    CHECK(near(unequal.band, 1.25 * (f(1.25) - f(1)) / f(1) / (1.25 * 1.25 - 1)));
    // A ratio from element counts: h as (volume / elements)^(1/3).
    CHECK(near(refinementRatio(1000, 8000), 2));
    // Moving towards a value, but at no order f0 + C h^p can give: the
    // coarse step (ratio 2) moved it only twice the fine step (ratio 1.1),
    // where any order above 0 asks for more than 7 times. Said, not
    // extrapolated. Likewise an order above 10, which no element gives.
    CHECK(convergence(10, 10.2, 10.3, 2, 1.1).kind == Convergence::Kind::Irregular);
    CHECK(convergence(10, 10.9, 11, 1.1, 2).kind == Convergence::Kind::Irregular);
    // Oscillating: up, then down.
    CHECK(convergence(10, 12, 11, 1.5, 1.5).kind == Convergence::Kind::Oscillating);
    // Diverging: a peak growing faster as the mesh is refined.
    const Convergence away = convergence(100, 150, 250, 1.5, 1.5);
    CHECK(away.kind == Convergence::Kind::Diverging && near(away.ratio, 2));
    // The same to the digits on every mesh.
    CHECK(convergence(5, 5, 5, 1.5, 1.5).kind == Convergence::Kind::Unchanged);
    // Changed only on the last mesh: no order to find (the coarse two
    // agree), so not called converging.
    CHECK(convergence(5, 5, 6, 1.5, 1.5).kind == Convergence::Kind::Diverging);

    CHECK(convergenceWords("largest displacement", "mm", f(2.25), f(1.5), f(1), 1.5, 1.5)
          == "largest displacement: 12 mm on the finest mesh; converging (order 2.0), 10 mm at zero element size, "
             "the finest mesh within 21 % of it (grid convergence index)");
    CHECK(convergenceWords("peak von Mises stress", "MPa", 100, 150, 250, 1.5, 1.5)
          == "peak von Mises stress: 250 MPa on the finest mesh; NOT converging: it moves more with each finer "
             "mesh (100, 150, 250 MPa). A value that grows without end as the mesh is refined comes from a "
             "sharp corner, or a load or support on an edge or a point: it has no converged value; read "
             "the result away from there, or round the corner");
    CHECK(convergenceWords("largest displacement", "mm", 10, 12, 11, 1.5, 1.5)
          == "largest displacement: 11 mm on the finest mesh; oscillating (10, 12, 11 mm): no order to "
             "extrapolate with; the finest two differ by 9.1 %");
    CHECK(convergenceWords("largest displacement", "mm", 10, 10.2, 10.3, 2, 1.1)
          == "largest displacement: 10.3 mm on the finest mesh; settling (10, 10.2, 10.3 mm), but not at a steady "
             "rate: no order to extrapolate with; the finest two differ by 0.97 %");
    CHECK(convergenceWords("largest displacement", "mm", 5, 5, 5, 1.5, 1.5)
          == "largest displacement: 5 mm on the finest mesh; the same on all three meshes");
    // The peaks of a results file's last time point: two increments, the
    // second's largest displacement 5 (3, 4, 0) and von Mises 250.
    {
        ResultsSeries series;
        for (int k = 1; k <= 2; ++k) {
            auto g = vtkSmartPointer<vtkUnstructuredGrid>::New();
            vtkNew<vtkPoints> pts;
            for (int i = 0; i < 4; ++i)
                pts->InsertNextPoint(i == 1, i == 2, i == 3);
            g->SetPoints(pts);
            const vtkIdType tet[4] = {0, 1, 2, 3};
            g->InsertNextCell(VTK_TETRA, 4, tet);
            vtkNew<vtkDoubleArray> disp;
            disp->SetName("DISP");
            disp->SetNumberOfComponents(3);
            vtkNew<vtkDoubleArray> mises;
            mises->SetName("von Mises stress (derived)");
            for (int i = 0; i < 4; ++i) {
                const double d[3] = {i == 2 ? 3.0 * k / 2 : 0.1, i == 2 ? 4.0 * k / 2 : 0.0, 0};
                disp->InsertNextTuple(d);
                mises->InsertNextValue(i == 3 ? 125.0 * k : 10.0);
            }
            g->GetPointData()->AddArray(disp);
            g->GetPointData()->AddArray(mises);
            series.points.push_back({double(k), g});
        }
        QTemporaryDir dir;
        QString error;
        const QString path = dir.filePath("results.vtkhdf");
        CHECK(writeVtkHdf(series, path, &error));
        const ResultPeaks peaks = resultPeaks(path);
        CHECK(peaks.error.isEmpty());
        CHECK(std::abs(peaks.displacement - 5) < 1e-6);
        CHECK(std::abs(peaks.vonMises - 250) < 1e-4);
        CHECK(!resultPeaks(dir.filePath("none.vtkhdf")).error.isEmpty());
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
