#include "core/TestCurve.h"

#include <QFileInfo>

#include <vtkDataArray.h>
#include <vtkDataAssembly.h>
#include <vtkHDFReader.h>
#include <vtkInformation.h>
#include <vtkNew.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPolyDataReader.h>

#include <cmath>
#include <string>

namespace gf::core {

TestCurve readTenSenseCurve(const QString &path, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return TestCurve{};
    };
    if (error)
        error->clear();
    vtkNew<vtkXMLPolyDataReader> reader;
    if (!QFileInfo(path).isFile() || !reader->CanReadFile(path.toUtf8().constData()))
        return fail("cannot read " + path + " as a TenSense curve (.vtp)");
    reader->SetFileName(path.toUtf8().constData());
    reader->Update();
    vtkPointData *pd = reader->GetOutput()->GetPointData();
    vtkDataArray *strain = pd->GetArray("strain"), *force = pd->GetArray("force_N");
    if (!strain || !force)
        return fail(path + ": no \"strain\" and \"force_N\" (not a TenSense analysed curve)");
    TestCurve c;
    for (vtkIdType i = 0; i < strain->GetNumberOfTuples(); ++i) {
        c.strain.push_back(strain->GetTuple1(i));
        c.force.push_back(force->GetTuple1(i));
    }
    c.source = "measured: " + QFileInfo(path).fileName() + " (TenSense's analysed curve)";
    return c;
}

namespace {

// A block of the results file at the reader's current time: /<root>/<path>.
vtkUnstructuredGrid *block(vtkHDFReader *reader, const std::string &path)
{
    auto *c = vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutputDataObject(0));
    vtkDataAssembly *a = c ? c->GetDataAssembly() : nullptr;
    if (!a)
        return nullptr;
    const int node = a->GetFirstNodeByPath(("/" + std::string(a->GetRootNodeName()) + "/" + path).c_str());
    const std::vector<unsigned int> sets = node < 0 ? std::vector<unsigned int>{} : a->GetDataSetIndices(node, false);
    return sets.size() == 1 ? vtkUnstructuredGrid::SafeDownCast(c->GetPartition(sets[0], 0)) : nullptr;
}

vtkDataArray *named(vtkPointData *pd, std::initializer_list<const char *> names)
{
    for (const char *n : names)
        if (vtkDataArray *d = pd->GetArray(n); d && d->GetNumberOfComponents() == 3)
            return d;
    return nullptr;
}

// Where a point of the part sits: its cell, and the weights of the cell's
// points there.
struct Probe {
    std::vector<vtkIdType> points;
    std::vector<double> weights;
};

} // namespace

TestCurve predictedCurve(const QString &resultsPath, const QString &group, const std::array<double, 3> &a,
                         const std::array<double, 3> &b, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return TestCurve{};
    };
    if (error)
        error->clear();
    if (!QFileInfo(resultsPath).isFile())
        return fail("cannot read " + resultsPath);
    vtkNew<vtkHDFReader> reader;
    reader->SetFileName(resultsPath.toUtf8().constData());
    reader->UpdateInformation();
    std::vector<double> times;
    vtkInformation *info = reader->GetOutputInformation(0);
    if (info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) {
        const double *t = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
        times.assign(t, t + info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS()));
    }
    const bool timed = !times.empty();
    if (!timed)
        times.push_back(0);  // a file of one time point may report none (VTK 9.5)

    // The set as the deck names it: CalculiX's names have no spaces.
    const std::string setPath = "NodeSets/" + group.toStdString(),
                      deckPath = "NodeSets/" + QString(group).replace(' ', '_').toStdString();
    const double length = std::sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1])
                                    + (b[2] - a[2]) * (b[2] - a[2]));
    if (!(length > 0))
        return fail("the extensometer's two points are the same point");
    const double along[3] = {(b[0] - a[0]) / length, (b[1] - a[1]) / length, (b[2] - a[2]) / length};

    TestCurve curve;
    curve.strain.push_back(0);
    curve.force.push_back(0);
    Probe probes[2];
    for (size_t k = 0; k < times.size(); ++k) {
        if (timed)
            reader->UpdateTimeStep(times[k]);
        else
            reader->Update();
        vtkUnstructuredGrid *model = block(reader, "Model");
        vtkUnstructuredGrid *set = block(reader, setPath);
        if (!set)
            set = block(reader, deckPath);
        if (!model)
            return fail(resultsPath + ": no Model block (not a GridForge results file)");
        if (!set)
            return fail(QString("no node set \"%1\" in the results").arg(group));
        vtkDataArray *u = named(model->GetPointData(), {"DISP", "U", "displacement"});
        vtkDataArray *rf = named(set->GetPointData(), {"FORC", "RF", "reaction"});
        if (!u)
            return fail("the results hold no displacements");
        if (!rf)
            return fail("the results hold no reactions; ask the step for its \"reaction\" output");
        if (k == 0) {
            // The gauge points found once, in the part as it was made.
            const std::array<double, 3> *at[2] = {&a, &b};
            for (int p = 0; p < 2; ++p) {
                double x[3] = {(*at[p])[0], (*at[p])[1], (*at[p])[2]}, pcoords[3];
                std::vector<double> weights(model->GetMaxCellSize());
                int sub = 0;
                const vtkIdType cell = model->FindCell(x, nullptr, -1, 1e-9, sub, pcoords, weights.data());
                if (cell < 0)
                    return fail(QString("the extensometer's point (%1, %2, %3) is outside the part")
                                    .arg(x[0]).arg(x[1]).arg(x[2]));
                vtkCell *c = model->GetCell(cell);
                for (vtkIdType i = 0; i < c->GetNumberOfPoints(); ++i) {
                    probes[p].points.push_back(c->GetPointId(i));
                    probes[p].weights.push_back(weights[i]);
                }
            }
        }
        double moved[2][3] = {};
        for (int p = 0; p < 2; ++p)
            for (size_t i = 0; i < probes[p].points.size(); ++i)
                for (int d = 0; d < 3; ++d)
                    moved[p][d] += probes[p].weights[i] * u->GetComponent(probes[p].points[i], d);
        double now = 0;
        for (int d = 0; d < 3; ++d) {
            const double span = b[d] + moved[1][d] - a[d] - moved[0][d];
            now += span * span;
        }
        double force = 0;
        for (vtkIdType i = 0; i < rf->GetNumberOfTuples(); ++i)
            for (int d = 0; d < 3; ++d)
                force += rf->GetComponent(i, d) * along[d];
        curve.strain.push_back(std::sqrt(now) / length - 1);
        curve.force.push_back(std::abs(force));
    }
    curve.source = QString("predicted: %1, the force on %2, an extensometer of %3 from (%4, %5, %6)")
                       .arg(QFileInfo(resultsPath).fileName(), group)
                       .arg(length)
                       .arg(a[0])
                       .arg(a[1])
                       .arg(a[2]);
    return curve;
}

CurveComparison compareCurves(const TestCurve &measured, const TestCurve &predicted)
{
    CurveComparison out;
    for (size_t k = 0; k < predicted.strain.size(); ++k) {
        const double e = predicted.strain[k];
        if (!(e > 0))
            continue;  // the zero point: nothing to compare
        bool found = false;
        for (size_t i = 1; i < measured.strain.size(); ++i) {
            const double s0 = measured.strain[i - 1], s1 = measured.strain[i];
            if (s0 < e && s1 >= e) {
                const double t = (e - s0) / (s1 - s0);
                out.points.push_back(
                    {e, predicted.force[k], measured.force[i - 1] + t * (measured.force[i] - measured.force[i - 1])});
                found = true;
                break;
            }
        }
        out.outside += found ? 0 : 1;
    }
    return out;
}

QString comparisonWords(const CurveComparison &c)
{
    auto pct = [](double x) { return QString::number(100 * x, 'g', 3) + " %"; };
    QString words;
    if (c.points.empty()) {
        words = "predicted against measured: no predicted point inside the measured strains";
    } else {
        const CurveComparison::Point *worst = &c.points.front();
        double mean = 0;
        for (const auto &p : c.points) {
            mean += std::abs(p.difference()) / c.points.size();
            if (std::abs(p.difference()) > std::abs(worst->difference()))
                worst = &p;
        }
        words = QString("predicted against measured, at %1 from %2 to %3: at most %4 apart (%5, at %6), %7 "
                        "on average")
                    .arg(c.points.size() == 1 ? QString("1 strain") : QString("%1 strains").arg(c.points.size()))
                    .arg(pct(c.points.front().strain), pct(c.points.back().strain), pct(std::abs(worst->difference())),
                         worst->difference() < 0 ? "under" : "over", pct(worst->strain), pct(mean));
    }
    if (c.outside > 0)
        words += QString("; %1 predicted %2 beyond the measured strains, not compared")
                     .arg(c.outside)
                     .arg(c.outside == 1 ? "point" : "points");
    return words;
}

QByteArray comparisonCsv(const TestCurve &measured, const TestCurve &predicted, const CurveComparison &c)
{
    QByteArray text = "# " + measured.source.toUtf8() + "\n# " + predicted.source.toUtf8()
                      + "\nstrain,predicted force (N),measured force (N),difference\n";
    for (const auto &p : c.points)
        text += QString("%1,%2,%3,%4\n")
                    .arg(p.strain, 0, 'g', 8)
                    .arg(p.predicted, 0, 'g', 8)
                    .arg(p.measured, 0, 'g', 8)
                    .arg(p.difference(), 0, 'g', 6)
                    .toUtf8();
    return text;
}

} // namespace gf::core
