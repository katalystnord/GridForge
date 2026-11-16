#include "core/Stress.h"

#include <vtkMath.h>

#include <cmath>

namespace gf::core {

double vonMises(const SymmetricTensor &s)
{
    const double a = s.xx - s.yy, b = s.yy - s.zz, c = s.zz - s.xx;
    return std::sqrt((a * a + b * b + c * c) / 2
                     + 3 * (s.xy * s.xy + s.yz * s.yz + s.zx * s.zx));
}

std::array<double, 3> principalValues(const SymmetricTensor &s)
{
    double m[3][3] = {{s.xx, s.xy, s.zx}, {s.xy, s.yy, s.yz}, {s.zx, s.yz, s.zz}};
    double v[3][3];
    double *mr[3] = {m[0], m[1], m[2]}, *vr[3] = {v[0], v[1], v[2]};
    double w[3];
    vtkMath::Jacobi(mr, w, vr);  // eigenvalues sorted largest first
    return {w[0], w[1], w[2]};
}

} // namespace gf::core
