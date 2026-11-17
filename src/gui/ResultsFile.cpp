#include "gui/ResultsFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <vtkDataAssembly.h>
#include <vtkDoubleArray.h>
#include <vtkHDFReader.h>
#include <vtkInformation.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkPointData.h>
#include <vtkStreamingDemandDrivenPipeline.h>

#include <cmath>

namespace gf::gui {

namespace {

const char *magnitude = " (magnitude)";

// The Model block of the file at the reader's current time (the writer
// puts it at /Results/Model; core::writeVtkHdf).
vtkUnstructuredGrid *modelBlock(vtkHDFReader *reader)
{
    auto *c = vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutputDataObject(0));
    vtkDataAssembly *a = c ? c->GetDataAssembly() : nullptr;
    if (!a)
        return nullptr;
    const int node = a->GetFirstNodeByPath((QString("/") + a->GetRootNodeName() + "/Model").toUtf8().constData());
    const std::vector<unsigned int> sets = node < 0 ? std::vector<unsigned int>{} : a->GetDataSetIndices(node, false);
    return sets.size() == 1 ? vtkUnstructuredGrid::SafeDownCast(c->GetPartition(sets[0], 0)) : nullptr;
}

} // namespace

ResultsFile::ResultsFile() = default;
ResultsFile::~ResultsFile() = default;

bool ResultsFile::open(const QString &path, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (!QFileInfo(path).isFile())
        return fail("cannot read " + path);
    m_reader = vtkSmartPointer<vtkHDFReader>::New();
    m_reader->SetFileName(path.toUtf8().constData());
    m_reader->UpdateInformation();
    vtkInformation *info = m_reader->GetOutputInformation(0);
    m_times.clear();
    if (info->Has(vtkStreamingDemandDrivenPipeline::TIME_STEPS())) {
        const int n = info->Length(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
        const double *t = info->Get(vtkStreamingDemandDrivenPipeline::TIME_STEPS());
        m_times.assign(t, t + n);
    }
    if (m_times.empty()) {
        // VTK 9.5's reader reports no time step for a file of one (nor its
        // value, though the file holds it). results.json beside it lists the
        // time points (the toolkit writes them); else 0.
        QFile report(QFileInfo(path).absoluteDir().filePath("results.json"));
        const QJsonArray listed = report.open(QIODevice::ReadOnly)
                                      ? QJsonDocument::fromJson(report.readAll()).object()["time points"].toArray()
                                      : QJsonArray();
        m_times.push_back(listed.size() == 1 ? listed.at(0).toDouble() : 0.0);
    }
    m_reader->UpdateTimeStep(m_times.front());
    vtkUnstructuredGrid *g = modelBlock(m_reader);
    if (!g)
        return fail(path + ": no Model block (not a GridForge results file)");
    m_fields.clear();
    vtkPointData *pd = g->GetPointData();
    for (int a = 0; a < pd->GetNumberOfArrays(); ++a) {
        vtkDataArray *d = pd->GetArray(a);
        if (!d || QString(d->GetName()) == "node id")
            continue;
        if (d->GetNumberOfComponents() == 1)
            m_fields << d->GetName();
        else if (d->GetNumberOfComponents() == 3)
            m_fields << QString(d->GetName()) + magnitude;
    }
    m_path = path;
    if (error)
        error->clear();
    return true;
}

QString ResultsFile::preferredField() const
{
    for (const char *f : {"PE", "von Mises stress (derived)"})
        if (m_fields.contains(f))
            return f;
    return m_fields.value(0);
}

vtkSmartPointer<vtkUnstructuredGrid> ResultsFile::model(int k, const QString &field)
{
    if (!m_reader || k < 0 || k >= timeCount())
        return nullptr;
    m_reader->UpdateTimeStep(m_times[k]);
    vtkUnstructuredGrid *g = modelBlock(m_reader);
    if (!g)
        return nullptr;
    auto out = vtkSmartPointer<vtkUnstructuredGrid>::New();
    out->ShallowCopy(g);
    const bool isMagnitude = field.endsWith(magnitude);
    const QString name = isMagnitude ? field.chopped(int(strlen(magnitude))) : field;
    vtkDataArray *d = out->GetPointData()->GetArray(name.toUtf8().constData());
    if (!d)
        return out;
    vtkNew<vtkDoubleArray> shown;
    shown->SetName("shown");
    shown->SetNumberOfValues(d->GetNumberOfTuples());
    for (vtkIdType i = 0; i < d->GetNumberOfTuples(); ++i) {
        double v = 0;
        if (isMagnitude) {
            for (int c = 0; c < d->GetNumberOfComponents(); ++c)
                v += d->GetComponent(i, c) * d->GetComponent(i, c);
            v = std::sqrt(v);
        } else {
            v = d->GetComponent(i, 0);
        }
        shown->SetValue(i, v);
    }
    out->GetPointData()->AddArray(shown);
    out->GetPointData()->SetActiveScalars("shown");
    return out;
}

std::pair<double, double> ResultsFile::range(int k, const QString &field)
{
    vtkSmartPointer<vtkUnstructuredGrid> g = model(k, field);
    vtkDataArray *shown = g ? g->GetPointData()->GetArray("shown") : nullptr;
    if (!shown)
        return {0, 0};
    double r[2];
    shown->GetRange(r);
    return {r[0], r[1]};
}

} // namespace gf::gui
