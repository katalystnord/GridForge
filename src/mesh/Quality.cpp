#include "mesh/Quality.h"

#include <QJsonArray>

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace gf::mesh {

namespace {

using V = std::array<double, 3>;

V sub(const V &a, const V &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const V &a, const V &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V cross(const V &a, const V &b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

const double pi = 3.14159265358979323846;

// Six times the signed volume: positive when corners 0 1 2 turn
// counter-clockwise seen from corner 3 (VTK's order).
double sixVolume(const V p[4]) { return dot(cross(sub(p[1], p[0]), sub(p[2], p[0])), sub(p[3], p[0])); }

double meanRatio(const V p[4])
{
    const double v = std::abs(sixVolume(p)) / 6;
    double edges = 0;
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            edges += dot(sub(p[i], p[j]), sub(p[i], p[j]));
    return edges > 0 ? 12 * std::pow(3 * v, 2.0 / 3.0) / edges : 0;
}

// The smallest angle between two faces, in degrees: 180 less the angle
// between their outward normals.
double minDihedral(const V p[4])
{
    V normal[4];
    for (int k = 0; k < 4; ++k) {
        int f[3], n = 0;
        for (int i = 0; i < 4; ++i)
            if (i != k)
                f[n++] = i;
        V x = cross(sub(p[f[1]], p[f[0]]), sub(p[f[2]], p[f[0]]));
        if (dot(x, sub(p[k], p[f[0]])) > 0)  // outward: away from the corner opposite
            x = {-x[0], -x[1], -x[2]};
        const double length = std::sqrt(dot(x, x));
        normal[k] = length > 0 ? V{x[0] / length, x[1] / length, x[2] / length} : V{0, 0, 0};
    }
    double worst = 180;
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            worst = std::min(worst, 180 - std::acos(std::clamp(dot(normal[i], normal[j]), -1.0, 1.0)) * 180 / pi);
    return worst;
}

// A quadratic tetrahedron (VTK's order: corners, then edges 01 12 20 03 13
// 23) whose Jacobian determinant is not positive, with the corners'
// orientation, at a corner, a middle node or the centre.
bool folded(const V p[10])
{
    static const int edge[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3}, {2, 3}};
    static const double at[11][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                                     {.5, 0, 0}, {.5, .5, 0}, {0, .5, 0}, {0, 0, .5},
                                     {.5, 0, .5}, {0, .5, .5}, {.25, .25, .25}};
    static const double dL[4][3] = {{-1, -1, -1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const double sign = sixVolume(p) >= 0 ? 1 : -1;
    for (const auto &r : at) {
        const double L[4] = {1 - r[0] - r[1] - r[2], r[0], r[1], r[2]};
        double dN[10][3];
        for (int k = 0; k < 4; ++k)
            for (int d = 0; d < 3; ++d)
                dN[k][d] = (4 * L[k] - 1) * dL[k][d];
        for (int k = 0; k < 6; ++k) {
            const int i = edge[k][0], j = edge[k][1];
            for (int d = 0; d < 3; ++d)
                dN[4 + k][d] = 4 * (L[i] * dL[j][d] + L[j] * dL[i][d]);
        }
        double J[3][3] = {};
        for (int n = 0; n < 10; ++n)
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                    J[a][b] += p[n][a] * dN[n][b];
        const double det = J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1])
                           - J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0])
                           + J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]);
        if (det * sign <= 0)
            return true;
    }
    return false;
}

QString count(long long n, const char *one, const char *many)
{
    return QString("%1 %2").arg(n).arg(n == 1 ? one : many);
}

} // namespace

Quality measure(vtkUnstructuredGrid *grid)
{
    Quality q;
    const vtkIdType cells = grid->GetNumberOfCells();
    q.meanRatio.assign(cells, -1);
    q.minMeanRatio = 1;
    q.minDihedral = 180;
    double sum = 0;
    for (vtkIdType c = 0; c < cells; ++c) {
        const int type = grid->GetCellType(c);
        if (type != VTK_TETRA && type != VTK_QUADRATIC_TETRA) {
            ++q.notMeasured;
            continue;
        }
        vtkIdType n;
        const vtkIdType *pts;
        grid->GetCellPoints(c, n, pts);
        V p[10];
        for (vtkIdType k = 0; k < n && k < 10; ++k)
            grid->GetPoint(pts[k], p[k].data());
        ++q.tetrahedra;
        q.inverted += sixVolume(p) <= 0 ? 1 : 0;
        if (type == VTK_QUADRATIC_TETRA && sixVolume(p) > 0)
            q.folded += folded(p) ? 1 : 0;
        const double ratio = meanRatio(p);
        q.meanRatio[c] = ratio;
        sum += ratio;
        q.minMeanRatio = std::min(q.minMeanRatio, ratio);
        q.meanRatioBelow01 += ratio < 0.1 ? 1 : 0;
        q.meanRatioBelow03 += ratio < 0.3 ? 1 : 0;
        q.minDihedral = std::min(q.minDihedral, minDihedral(p));
    }
    if (q.tetrahedra == 0) {
        q.minMeanRatio = q.minDihedral = 0;
        return q;
    }
    q.averageMeanRatio = sum / static_cast<double>(q.tetrahedra);
    std::vector<vtkIdType> order;
    for (vtkIdType c = 0; c < cells; ++c)
        if (q.meanRatio[c] >= 0)
            order.push_back(c);
    const size_t keep = std::min<size_t>(5, order.size());
    std::partial_sort(order.begin(), order.begin() + static_cast<long>(keep), order.end(),
                      [&](vtkIdType a, vtkIdType b) {
                          return q.meanRatio[a] < q.meanRatio[b]
                                 || (q.meanRatio[a] == q.meanRatio[b] && a < b);
                      });
    q.worst.assign(order.begin(), order.begin() + static_cast<long>(keep));
    return q;
}

QJsonObject toJson(const Quality &q, vtkUnstructuredGrid *grid)
{
    vtkDataArray *ids = grid ? grid->GetCellData()->GetArray("element id") : nullptr;
    QJsonArray worst;
    for (vtkIdType c : q.worst) {
        QJsonObject cell;
        if (ids)
            cell["element id"] = static_cast<qint64>(ids->GetTuple1(c));
        else
            cell["cell"] = static_cast<qint64>(c);
        cell["mean ratio"] = q.meanRatio[c];
        worst.append(cell);
    }
    return {
        {"format", "gridforge-mesh-quality"},
        {"version", "1.0"},
        {"solver ready", q.solverReady()},
        {"tetrahedra", q.tetrahedra},
        {"not measured", q.notMeasured},
        {"inverted", q.inverted},
        {"folded", q.folded},
        {"mean ratio", QJsonObject{{"min", q.minMeanRatio},
                                   {"average", q.averageMeanRatio},
                                   {"below 0.3", q.meanRatioBelow03},
                                   {"below 0.1", q.meanRatioBelow01}}},
        {"smallest dihedral angle", q.minDihedral},
        {"worst", worst},
    };
}

QStringList summary(const Quality &q)
{
    QStringList lines;
    if (q.solverReady()) {
        lines << "ready for a solver: no inverted or folded elements";
    } else {
        QStringList faults;
        if (q.inverted)
            faults << count(q.inverted, "inverted element", "inverted elements");
        if (q.folded)
            faults << count(q.folded, "folded element", "folded elements");
        lines << "NOT ready for a solver: " + faults.join(", ");
    }
    if (q.tetrahedra > 0) {
        lines << QString("%1; mean ratio %2 at worst, %3 on average; smallest dihedral angle %4 degrees")
                     .arg(count(q.tetrahedra, "tetrahedron", "tetrahedra"))
                     .arg(q.minMeanRatio, 0, 'f', 3)
                     .arg(q.averageMeanRatio, 0, 'f', 3)
                     .arg(q.minDihedral, 0, 'f', 1);
        auto some = [](long long n) { return n ? QString::number(n) : QString("none"); };
        lines << QString("below 0.3: %1; below 0.1: %2").arg(some(q.meanRatioBelow03), some(q.meanRatioBelow01));
    }
    if (q.notMeasured > 0)
        lines << count(q.notMeasured, "cell", "cells") + " not measured (version 1 measures tetrahedra)";
    return lines;
}

} // namespace gf::mesh
