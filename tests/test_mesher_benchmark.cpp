// The meshing study's benchmark, kept as a test (docs/survey/meshing.md,
// section 7, item 4): GridForge's mesher on the study's models at the
// study's sizes, quadratic, measured against OpenCASCADE's own volume and
// outer area of each (tools/mesh-bench/models/models.json).
//
// It fails on what a solver would refuse (an inverted or folded element),
// on a mesh that misses its geometry (volume or outside area off), and on
// any measure worse than the committed baseline
// (tools/mesh-bench/gridforge-baseline.json). Everything else is reported:
// the table is printed and written as JSON beside the test, for the
// release notes (tenet 9).
//
//   test_mesher_benchmark                  run, compare, report
//   test_mesher_benchmark --baseline FILE  also write the numbers to FILE

#include "mesh/Mesher.h"
#include "mesh/Quality.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <vtkCellType.h>

#include <unistd.h>
#include <vtkUnstructuredGrid.h>
#include <vtkNew.h>
#include <vtkIdList.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#ifndef GF_BENCH
#error "GF_BENCH must point at tools/mesh-bench"
#endif

namespace {

using V = std::array<double, 3>;

V at(vtkUnstructuredGrid *g, vtkIdType p)
{
    V x;
    g->GetPoint(p, x.data());
    return x;
}

// The volume of quadratic tetrahedra with their middle nodes where they
// are: the same 4-point rule as the study's referee (evaluate.py), so the
// numbers compare; exact for straight cells.
double curvedVolume(vtkUnstructuredGrid *g)
{
    static const int edge[6][2] = {{0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3}, {2, 3}};
    static const double dL[4][3] = {{-1, -1, -1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const double a = 0.5854101966249685, b = 0.1381966011250105;
    const double rule[4][3] = {{a, b, b}, {b, a, b}, {b, b, a}, {b, b, b}};
    double total = 0;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        if (g->GetCellType(c) != VTK_QUADRATIC_TETRA)
            continue;
        vtkIdType n;
        const vtkIdType *pts;
        g->GetCellPoints(c, n, pts);
        for (const auto &r : rule) {
            const double L[4] = {1 - r[0] - r[1] - r[2], r[0], r[1], r[2]};
            double J[3][3] = {};
            for (int k = 0; k < 10; ++k) {
                double d[3];
                for (int j = 0; j < 3; ++j)
                    d[j] = k < 4 ? (4 * L[k] - 1) * dL[k][j]
                                 : 4 * (L[edge[k - 4][0]] * dL[edge[k - 4][1]][j]
                                        + L[edge[k - 4][1]] * dL[edge[k - 4][0]][j]);
                const V x = at(g, pts[k]);
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        J[i][j] += x[i] * d[j];
            }
            total += (J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1])
                      - J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0])
                      + J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]))
                     / 24;
        }
    }
    return total;
}

// The area of the faces (corner triangles) used by one tetrahedron only:
// the outside, plus any face inside the mesh whose two sides do not share
// nodes (so two parts not joined show as too much area).
double outerArea(vtkUnstructuredGrid *g)
{
    std::map<std::array<vtkIdType, 3>, std::pair<int, std::array<vtkIdType, 3>>> faces;
    static const int f[4][3] = {{1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        vtkIdType n;
        const vtkIdType *pts;
        g->GetCellPoints(c, n, pts);
        for (const auto &face : f) {
            std::array<vtkIdType, 3> t{pts[face[0]], pts[face[1]], pts[face[2]]}, key = t;
            std::sort(key.begin(), key.end());
            auto &entry = faces[key];
            entry.first += 1;
            entry.second = t;
        }
    }
    double area = 0;
    for (const auto &[key, entry] : faces) {
        if (entry.first != 1)
            continue;
        const V p = at(g, entry.second[0]), q = at(g, entry.second[1]), r = at(g, entry.second[2]);
        const V u{q[0] - p[0], q[1] - p[1], q[2] - p[2]}, v{r[0] - p[0], r[1] - p[1], r[2] - p[2]};
        const V x{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        area += 0.5 * std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    }
    return area;
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject{};
}

// The mesh itself, as one SHA-256: each node to 12 significant figures,
// then each cell's type and nodes. The same geometry and recipe must give
// the same mesh on every machine (tenet 10); 12 figures are far finer
// than any measure here and spare the last bit of a different libm.
QString meshHash(vtkUnstructuredGrid *g)
{
    QCryptographicHash h(QCryptographicHash::Sha256);
    char line[128];
    for (vtkIdType p = 0; p < g->GetNumberOfPoints(); ++p) {
        double x[3];
        g->GetPoint(p, x);
        const int n = std::snprintf(line, sizeof line, "%.12g %.12g %.12g\n", x[0], x[1], x[2]);
        h.addData(QByteArrayView(line, n));
    }
    vtkNew<vtkIdList> ids;
    for (vtkIdType c = 0; c < g->GetNumberOfCells(); ++c) {
        g->GetCellPoints(c, ids);
        int n = std::snprintf(line, sizeof line, "%d", g->GetCellType(c));
        h.addData(QByteArrayView(line, n));
        for (vtkIdType k = 0; k < ids->GetNumberOfIds(); ++k) {
            n = std::snprintf(line, sizeof line, " %lld", static_cast<long long>(ids->GetId(k)));
            h.addData(QByteArrayView(line, n));
        }
        h.addData(QByteArrayView("\n", 1));
    }
    return QString::fromLatin1(h.result().toHex());
}

int failures = 0;
void fail(const QString &model, const QString &why)
{
    std::printf("  FAIL %s: %s\n", qPrintable(model), qPrintable(why));
    ++failures;
}

} // namespace

int main(int argc, char *argv[])
{
    // The same maths routines on every machine. glibc picks its sin, exp,
    // pow, ... by the processor (FMA with AVX2, FMA4, or plain), and with
    // FMA hidden here four of the five meshes moved (2026-11-18), as
    // GitHub's machine's do. Run again with the plain routines, so the
    // hashes compare across machines (docs/survey/netgen-review.md, item 6).
    const char *plain = "glibc.cpu.hwcaps=-AVX2,-FMA,-FMA4";
    if (qEnvironmentVariable("GLIBC_TUNABLES") != plain) {
        setenv("GLIBC_TUNABLES", plain, 1);
        execv("/proc/self/exe", argv);
        std::printf("could not run again with %s\n", plain);
        return 1;
    }
    std::printf("maths: %s\n", plain);
    const QString bench = GF_BENCH;
    const QJsonObject models = readJson(bench + "/models/models.json");
    const QJsonObject baseline = readJson(bench + "/gridforge-baseline.json")["models"].toObject();
    const QString baselineOut = argc == 3 && std::string(argv[1]) == "--baseline" ? argv[2] : QString();
    if (models.isEmpty()) {
        std::printf("FAIL cannot read %s/models/models.json\n", qPrintable(bench));
        return 1;
    }

    // The study's sizes (tools/mesh-bench/run.py).
    const std::vector<std::pair<QString, double>> runs = {
        {"dogbone", 2.0}, {"bracket", 3.0}, {"thinwall", 1.5}, {"twoparts", 3.0}, {"duct", 2.0}};

    QJsonObject results;
    std::printf("%-9s %5s %8s %8s %11s %11s %9s %9s %7s %7s\n", "model", "size", "cells", "nodes",
                "volume err", "outside err", "min ratio", "min angle", "ready", "seconds");
    for (const auto &[name, size] : runs) {
        const QJsonObject ref = models[name].toObject();
        QString error;
        QElapsedTimer timer;
        timer.start();
        const auto s = gf::mesh::meshStep(bench + "/models/" + name + ".step", {size, 2}, &error);
        const double seconds = timer.elapsed() / 1000.0;
        if (!error.isEmpty() || s.points.empty()) {
            fail(name, "not meshed: " + error);
            continue;
        }
        vtkUnstructuredGrid *g = s.points.front().grid;
        const gf::mesh::Quality q = gf::mesh::measure(g);
        const double volumeError = curvedVolume(g) / ref["volume"].toDouble() - 1;
        const double outsideError = outerArea(g) / ref["outer_area"].toDouble() - 1;
        std::printf("%-9s %5.1f %8lld %8lld %+11.2e %+11.2e %9.4f %9.2f %7s %7.2f\n", qPrintable(name), size,
                    static_cast<long long>(g->GetNumberOfCells()),
                    static_cast<long long>(g->GetNumberOfPoints()), volumeError, outsideError,
                    q.minMeanRatio, q.minDihedral, q.solverReady() ? "yes" : "NO", seconds);
        results[name] = QJsonObject{{"size", size},
                                    {"order", 2},
                                    {"cells", static_cast<qint64>(g->GetNumberOfCells())},
                                    {"nodes", static_cast<qint64>(g->GetNumberOfPoints())},
                                    {"volume error", volumeError},
                                    {"outside error", outsideError},
                                    {"min mean ratio", q.minMeanRatio},
                                    {"average mean ratio", q.averageMeanRatio},
                                    {"min dihedral", q.minDihedral},
                                    {"solver ready", q.solverReady()},
                                    {"mesh hash", meshHash(g)},
                                    {"seconds", seconds}};

        // What a mesh must be, whatever the baseline.
        if (!q.solverReady())
            fail(name, QString("%1 inverted, %2 folded").arg(q.inverted).arg(q.folded));
        if (std::abs(volumeError) > 1e-4)
            fail(name, QString("volume off by %1").arg(volumeError));
        // Straight facets cut curved faces a little; a face left inside
        // (parts not joined) adds a tenth.
        if (std::abs(outsideError) > 5e-3)
            fail(name, QString("outside area off by %1").arg(outsideError));

        // No worse than the baseline.
        const QJsonObject was = baseline[name].toObject();
        if (was.isEmpty()) {
            std::printf("  (%s: no baseline yet)\n", qPrintable(name));
            continue;
        }
        // The worst of tens of thousands of elements moves by a few
        // hundredths from machine to machine (the duct's: 0.449 on GitHub's,
        // 0.495 here, 2026-11-18); that is not an error. A real loss is
        // larger, and an element a solver cannot take fails above.
        if (q.minMeanRatio < was["min mean ratio"].toDouble() - 0.1)
            fail(name, QString("worst mean ratio %1, was %2").arg(q.minMeanRatio).arg(was["min mean ratio"].toDouble()));
        if (q.averageMeanRatio < was["average mean ratio"].toDouble() - 0.01)
            fail(name, QString("average mean ratio %1, was %2").arg(q.averageMeanRatio).arg(was["average mean ratio"].toDouble()));
        if (std::abs(volumeError) > 2 * std::abs(was["volume error"].toDouble()) + 1e-7)
            fail(name, QString("volume error %1, was %2").arg(volumeError).arg(was["volume error"].toDouble()));
        if (std::abs(outsideError) > 2 * std::abs(was["outside error"].toDouble()) + 1e-6)
            fail(name, QString("outside error %1, was %2").arg(outsideError).arg(was["outside error"].toDouble()));
        // The same mesh as the committed one, node for node. A change
        // meant (a new Netgen pin, a new recipe) writes the baseline again
        // (--baseline), and says so in its commit.
        // Where GF_MESH_HASH_WARN is set (CI), a different mesh is a note,
        // not a failure: another processor rounds a little differently and
        // meshes a little differently, which is not an error (David,
        // 2026-11-18). The hash holds a mesh to itself on one machine.
        if (was.contains("mesh hash") && was["mesh hash"].toString() != meshHash(g)) {
            const QString why = QString("the mesh is not the committed one (%1 cells, the baseline %2): if that is "
                                        "meant, write the baseline again")
                                    .arg(g->GetNumberOfCells())
                                    .arg(was["cells"].toInteger());
            if (qEnvironmentVariableIsSet("GF_MESH_HASH_WARN"))
                std::printf("::warning title=Mesh differs on this machine::%s: %s\n", qPrintable(name), qPrintable(why));
            else
                fail(name, why);
        }
        if (g->GetNumberOfCells() != was["cells"].toInteger())
            std::printf("  (%s: %lld cells, the baseline has %lld: the mesh changed)\n", qPrintable(name),
                        static_cast<long long>(g->GetNumberOfCells()),
                        static_cast<long long>(was["cells"].toInteger()));
    }

    const QJsonObject report{{"format", "gridforge-mesh-benchmark"},
                             {"version", "1.0"},
                             {"mesher", gf::mesh::engine()},
                             {"models", results}};
    for (const QString &path : {QString("mesher-benchmark.json"), baselineOut}) {
        if (path.isEmpty())
            continue;
        QSaveFile out(path);
        if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(report).toJson()) < 0 || !out.commit())
            fail("report", "cannot write " + path);
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
