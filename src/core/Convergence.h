#pragma once

// How far a result can be trusted against the mesh (tenet 9): the same
// study solved on three meshes, each finer by the same ratio, and a value
// taken from each. Richardson extrapolation and Roache's grid convergence
// index, as the procedure of Celik et al. (2008, ASME J. Fluids Eng.
// 130(7) 078001) gives them for a constant refinement ratio r:
//
//   R  = (f2 - f1) / (f3 - f2)     f1 finest, f2 medium, f3 coarsest
//   p  = ln|(f3 - f2) / (f2 - f1)| / ln r          the apparent order
//   f0 = f1 + (f1 - f2) / (r^p - 1)                at zero element size
//   GCI = 1.25 |(f2 - f1) / f1| / (r^p - 1)        the finest mesh's band
//
// 0 < R < 1 converges steadily, -1 < R < 0 oscillates, R > 1 (or R < -1)
// moves away: a value that grows without end as the mesh is refined (a
// peak stress at a sharp corner, or under a load on an edge or a point)
// has no converged value to report.

#include <QString>

namespace gf::core {

struct Convergence {
    enum class Kind { Converging, Irregular, Oscillating, Diverging, Unchanged } kind = Kind::Unchanged;
    double ratio = 0;         // R
    double order = 0;         // p, when converging
    double extrapolated = 0;  // f0, when converging
    double band = 0;          // the finest mesh's GCI, a fraction, when converging
};

// From the three values, coarsest first, and the ratios between the
// meshes' element sizes (> 1): coarse to medium, then medium to fine.
// With the two ratios equal, the formulas above; with them unequal, as
// meshes give them, the order is the root of
//   (f3 - f2) / (f2 - f1) = r21^p (r32^p - 1) / (r21^p - 1),
// which is f = f0 + C h^p written for h1 = 1, h2 = r21, h3 = r21 r32
// (and the formula above when they are equal), and the rest uses r21.
// Irregular where no order between 0 and 10 fits.
Convergence convergence(double coarse, double medium, double fine, double rCoarse, double rFine);

// The ratio between two meshes' element sizes, from their element counts
// over the same volume: h as (volume / elements)^(1/3), as NASA's NPARC
// tutorial and Celik et al. take it for unstructured meshes.
double refinementRatio(double coarseElements, double fineElements);

// The verdict in words: "largest displacement: 1.2345 mm on the finest
// mesh; converging (order 1.9), 1.2391 mm at zero element size, the
// finest mesh within 0.42 % of it".
QString convergenceWords(const QString &quantity, const QString &unit, double coarse, double medium, double fine,
                         double rCoarse, double rFine);

// What a convergence check compares, from a results file as GridForge
// writes it (core::writeVtkHdf): at its last time point, the largest
// displacement (its magnitude) and the largest von Mises stress. -1 where
// the file holds no such field; error set if it cannot be read.
struct ResultPeaks {
    double displacement = -1;
    double vonMises = -1;
    QString error;
};
ResultPeaks resultPeaks(const QString &path);

} // namespace gf::core
