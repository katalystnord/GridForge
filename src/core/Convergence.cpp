#include "core/Convergence.h"

#include <QFileInfo>

#include <vtkDataAssembly.h>
#include <vtkDataArray.h>
#include <vtkHDFReader.h>
#include <vtkInformation.h>
#include <vtkNew.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkPointData.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <string>
#include <cmath>

namespace gf::core {

Convergence convergence(double coarse, double medium, double fine, double rCoarse, double rFine)
{
    Convergence c;
    const double finer = medium - fine;      // f2 - f1
    const double coarser = coarse - medium;  // f3 - f2
    // Differences below what single precision results keep are none.
    const double scale = std::max({std::abs(coarse), std::abs(medium), std::abs(fine)});
    const double none = 1e-6 * scale;
    if (std::abs(finer) <= none && std::abs(coarser) <= none) {
        c.kind = Convergence::Kind::Unchanged;
        return c;
    }
    if (std::abs(coarser) <= none) {
        // The coarse two agree and the fine one moved: no order to find.
        c.kind = Convergence::Kind::Diverging;
        c.ratio = INFINITY;
        return c;
    }
    c.ratio = finer / coarser;
    if (c.ratio <= -1 || c.ratio >= 1) {
        c.kind = Convergence::Kind::Diverging;
        return c;
    }
    if (c.ratio < 0) {
        c.kind = Convergence::Kind::Oscillating;
        return c;
    }
    c.kind = Convergence::Kind::Converging;
    if (std::abs(finer) <= none) {
        // Settled already on the medium mesh.
        c.order = INFINITY;
        c.extrapolated = fine;
        c.band = 0;
        return c;
    }
    const double steps = coarser / finer;  // 1 / R, above 1
    if (std::abs(rCoarse - rFine) <= 1e-12 * rFine) {
        c.order = std::log(steps) / std::log(rFine);
    } else {
        // f = f0 + C h^p for h = 1, rFine, rFine rCoarse: the steps' ratio
        // as a function of p rises from ln rCoarse / ln rFine (p -> 0).
        auto stepsAt = [&](double p) {
            const double a = std::pow(rFine, p);
            return a * (std::pow(rCoarse, p) - 1) / (a - 1);
        };
        double lo = 1e-9, hi = 10;
        if (!(stepsAt(lo) < steps && steps < stepsAt(hi))) {
            c.kind = Convergence::Kind::Irregular;
            return c;
        }
        for (int i = 0; i < 200; ++i) {
            const double mid = (lo + hi) / 2;
            (stepsAt(mid) < steps ? lo : hi) = mid;
        }
        c.order = (lo + hi) / 2;
    }
    if (!(c.order > 0 && c.order <= 10)) {
        c.kind = Convergence::Kind::Irregular;
        return c;
    }
    const double rp = std::pow(rFine, c.order);
    c.extrapolated = fine + (fine - medium) / (rp - 1);
    c.band = 1.25 * std::abs(finer / fine) / (rp - 1);
    return c;
}

double refinementRatio(double coarseElements, double fineElements)
{
    return std::cbrt(fineElements / coarseElements);
}

QString convergenceWords(const QString &quantity, const QString &unit, double coarse, double medium, double fine,
                         double rCoarse, double rFine)
{
    auto v = [&](double x) { return QString::number(x, 'g', 5); };
    auto pct = [](double x) { return QString::number(100 * x, 'g', 2) + " %"; };
    const QString head = QString("%1: %2 %3 on the finest mesh; ").arg(quantity, v(fine), unit);
    const QString three = QString("(%1, %2, %3 %4)").arg(v(coarse), v(medium), v(fine), unit);
    const QString finestTwo = pct(std::abs((medium - fine) / fine));
    const Convergence c = convergence(coarse, medium, fine, rCoarse, rFine);
    switch (c.kind) {
    case Convergence::Kind::Unchanged:
        return head + "the same on all three meshes";
    case Convergence::Kind::Oscillating:
        return head + QString("oscillating %1: no order to extrapolate with; the finest two differ by %2")
                          .arg(three, finestTwo);
    case Convergence::Kind::Irregular:
        return head + QString("settling %1, but not at a steady rate: no order to extrapolate with; the finest "
                              "two differ by %2")
                          .arg(three, finestTwo);
    case Convergence::Kind::Diverging:
        return head + QString("NOT converging: it moves more with each finer mesh %1. A value that grows without "
                              "end as the mesh is refined comes from a sharp corner, or a load or support on an "
                              "edge or a point: it has no converged value; read the result away from there, or "
                              "round the corner")
                          .arg(three);
    case Convergence::Kind::Converging:
        if (std::isinf(c.order))
            return head + "converged: the medium and finest meshes agree";
        return head + QString("converging (order %1), %2 %3 at zero element size, the finest mesh within %4 of it "
                              "(grid convergence index)")
                          .arg(QString::number(c.order, 'f', 1), v(c.extrapolated), unit, pct(c.band));
    }
    return head;
}

ResultPeaks resultPeaks(const QString &path)
{
    ResultPeaks peaks;
    if (!QFileInfo(path).isFile()) {
        peaks.error = "cannot read " + path;
        return peaks;
    }
    vtkNew<vtkHDFReader> reader;
    reader->SetFileName(path.toUtf8().constData());
    reader->UpdateInformation();
    // The last time point; a file of one may report none (VTK 9.5), and
    // then the one it holds is read.
    vtkInformation *info = reader->GetOutputInformation(0);
    if (info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) {
        const int n = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
        if (n > 0)
            reader->UpdateTimeStep(info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS())[n - 1]);
        else
            reader->Update();
    } else {
        reader->Update();
    }
    // The Model block (/<root>/Model, as core::writeVtkHdf puts it).
    auto *c = vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutputDataObject(0));
    vtkDataAssembly *a = c ? c->GetDataAssembly() : nullptr;
    const int node = a ? a->GetFirstNodeByPath((std::string("/") + a->GetRootNodeName() + "/Model").c_str()) : -1;
    const std::vector<unsigned int> sets = node < 0 ? std::vector<unsigned int>{} : a->GetDataSetIndices(node, false);
    auto *g = sets.size() == 1 ? vtkUnstructuredGrid::SafeDownCast(c->GetPartition(sets[0], 0)) : nullptr;
    if (!g) {
        peaks.error = path + ": no Model block (not a GridForge results file)";
        return peaks;
    }
    vtkPointData *pd = g->GetPointData();
    for (const char *name : {"DISP", "U", "displacement"}) {
        vtkDataArray *d = pd->GetArray(name);
        if (!d || d->GetNumberOfComponents() != 3)
            continue;
        for (vtkIdType i = 0; i < d->GetNumberOfTuples(); ++i) {
            const double *v = d->GetTuple3(i);
            peaks.displacement = std::max(peaks.displacement, std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
        }
        break;
    }
    if (vtkDataArray *m = pd->GetArray("von Mises stress (derived)"))
        peaks.vonMises = m->GetRange(0)[1];
    return peaks;
}

} // namespace gf::core
