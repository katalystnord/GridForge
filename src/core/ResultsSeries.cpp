#include "core/ResultsSeries.h"

#include <QDir>
#include <QSaveFile>

#include <QFile>
#include <QFileInfo>

#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCommand.h>
#include <vtkDataAssembly.h>
#include <vtkFieldData.h>
#include <vtkFloatArray.h>
#include <vtkHDFWriter.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPartitionedDataSetCollection.h>
#include <vtkPartitionedDataSetCollectionAlgorithm.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkStringArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <unordered_map>

namespace gf::core {

QStringList writeVtuSeries(const ResultsSeries &series, const QString &dir, const QString &base,
                           QString *error)
{
    error->clear();
    if (!QDir().mkpath(dir)) {
        *error = QString("cannot make the folder %1").arg(dir);
        return {};
    }
    QStringList written;
    QString pvd = "<?xml version=\"1.0\"?>\n"
                  "<VTKFile type=\"Collection\" version=\"0.1\" byte_order=\"LittleEndian\">\n"
                  "  <Collection>\n";
    for (size_t i = 0; i < series.points.size(); ++i) {
        const TimePoint &t = series.points[i];
        const QString name = QString("%1_%2.vtu").arg(base).arg(i + 1, 4, 10, QChar('0'));
        const QString path = QDir(dir).filePath(name);

        // The provenance travels in every file, not only in the series.
        vtkNew<vtkStringArray> provenance;
        provenance->SetName("provenance");
        for (const QString &line : series.provenance)
            provenance->InsertNextValue(line.toStdString());
        vtkNew<vtkUnstructuredGrid> grid;
        grid->ShallowCopy(t.grid);
        vtkNew<vtkFieldData> fields;
        fields->DeepCopy(t.grid->GetFieldData());
        fields->AddArray(provenance);
        grid->SetFieldData(fields);

        vtkNew<vtkXMLUnstructuredGridWriter> writer;
        writer->SetFileName(path.toLocal8Bit().constData());
        writer->SetInputData(grid);
        writer->SetDataModeToAppended();
        writer->SetCompressorTypeToZLib();
        if (writer->Write() != 1) {
            *error = QString("cannot write %1").arg(path);
            return {};
        }
        written << path;
        pvd += QString("    <DataSet timestep=\"%1\" group=\"\" part=\"0\" file=\"%2\"/>\n")
                   .arg(QString::number(t.time, 'g', 17), name.toHtmlEscaped());
    }
    pvd += "  </Collection>\n</VTKFile>\n";

    const QString path = QDir(dir).filePath(base + ".pvd");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(pvd.toUtf8()) < 0 || !file.commit()) {
        *error = QString("cannot write %1: %2").arg(path, file.errorString());
        return {};
    }
    written.prepend(path);
    return written;
}

namespace {

// The series as a pipeline source: it reports every time point and hands
// the writer the collection for the one it asks for.
class SeriesSource : public vtkPartitionedDataSetCollectionAlgorithm {
public:
    static SeriesSource *New();
    vtkTypeMacro(SeriesSource, vtkPartitionedDataSetCollectionAlgorithm);

    std::vector<double> times;
    std::vector<vtkSmartPointer<vtkPartitionedDataSetCollection>> collections;

protected:
    SeriesSource() { SetNumberOfInputPorts(0); }

    int RequestInformation(vtkInformation *, vtkInformationVector **,
                           vtkInformationVector *out) override
    {
        vtkInformation *info = out->GetInformationObject(0);
        info->Set(vtkStreamingDemandDrivenPipeline::TIME_STEPS(), times.data(), int(times.size()));
        const double range[2] = {times.front(), times.back()};
        info->Set(vtkStreamingDemandDrivenPipeline::TIME_RANGE(), range, 2);
        return 1;
    }

    int RequestData(vtkInformation *, vtkInformationVector **, vtkInformationVector *out) override
    {
        vtkInformation *info = out->GetInformationObject(0);
        size_t k = 0;
        if (info->Has(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP())) {
            const double t = info->Get(vtkStreamingDemandDrivenPipeline::UPDATE_TIME_STEP());
            for (size_t j = 0; j < times.size(); ++j)
                if (times[j] <= t)
                    k = j;
        }
        auto *c = vtkPartitionedDataSetCollection::GetData(out);
        c->ShallowCopy(collections[k]);
        c->GetInformation()->Set(vtkDataObject::DATA_TIME_STEP(), times[k]);
        return 1;
    }
};
vtkStandardNewMacro(SeriesSource);

// A set's own mesh, built once and shared by every time point, so the
// writer sees one unchanged mesh and stores it once.
struct SetMesh {
    std::vector<vtkIdType> points;  // the full grid's point behind each of the set's points
    std::vector<vtkIdType> cells;   // the full grid's cell behind each (element sets)
    vtkSmartPointer<vtkPoints> coords;
    vtkSmartPointer<vtkUnsignedCharArray> types;
    vtkSmartPointer<vtkCellArray> connectivity;
};

SetMesh meshOf(const NamedSet &set, vtkUnstructuredGrid *g)
{
    SetMesh m;
    m.coords = vtkSmartPointer<vtkPoints>::New();
    m.coords->SetDataTypeToDouble();
    m.types = vtkSmartPointer<vtkUnsignedCharArray>::New();
    m.connectivity = vtkSmartPointer<vtkCellArray>::New();
    if (set.nodes) {
        // A node set as one vertex at each of its points.
        for (vtkIdType p : set.members) {
            const vtkIdType id = vtkIdType(m.points.size());
            m.points.push_back(p);
            m.connectivity->InsertNextCell(1, &id);
            m.types->InsertNextValue(VTK_VERTEX);
        }
    } else {
        std::unordered_map<vtkIdType, vtkIdType> local;
        for (vtkIdType c : set.members) {
            vtkIdType n = 0;
            const vtkIdType *pts = nullptr;
            g->GetCellPoints(c, n, pts);
            std::vector<vtkIdType> ids;
            for (vtkIdType k = 0; k < n; ++k) {
                auto [it, fresh] = local.try_emplace(pts[k], vtkIdType(m.points.size()));
                if (fresh)
                    m.points.push_back(pts[k]);
                ids.push_back(it->second);
            }
            m.connectivity->InsertNextCell(n, ids.data());
            m.types->InsertNextValue(static_cast<unsigned char>(g->GetCellType(c)));
            m.cells.push_back(c);
        }
    }
    for (vtkIdType p : m.points)
        m.coords->InsertNextPoint(g->GetPoint(p));
    return m;
}

vtkSmartPointer<vtkUnstructuredGrid> setGrid(const SetMesh &m, vtkUnstructuredGrid *g,
                                             vtkFieldData *fields)
{
    auto b = vtkSmartPointer<vtkUnstructuredGrid>::New();
    b->SetPoints(m.coords);
    b->SetCells(m.types, m.connectivity);
    vtkPointData *from = g->GetPointData();
    b->GetPointData()->CopyAllocate(from, vtkIdType(m.points.size()));
    for (size_t i = 0; i < m.points.size(); ++i)
        b->GetPointData()->CopyData(from, m.points[i], vtkIdType(i));
    if (!m.cells.empty()) {
        vtkCellData *cfrom = g->GetCellData();
        b->GetCellData()->CopyAllocate(cfrom, vtkIdType(m.cells.size()));
        for (size_t i = 0; i < m.cells.size(); ++i)
            b->GetCellData()->CopyData(cfrom, m.cells[i], vtkIdType(i));
    }
    b->SetFieldData(fields);
    return b;
}

// The grid with its double fields as single-precision copies (the ids and
// the points stay as they are). Solver results are not more precise than
// that: ccx's .frd keeps six digits, its binary form is single precision.
vtkSmartPointer<vtkUnstructuredGrid> singlePrecision(vtkUnstructuredGrid *g)
{
    auto out = vtkSmartPointer<vtkUnstructuredGrid>::New();
    out->ShallowCopy(g);
    for (vtkDataSetAttributes *data : {static_cast<vtkDataSetAttributes *>(out->GetPointData()),
                                       static_cast<vtkDataSetAttributes *>(out->GetCellData())}) {
        for (int a = data->GetNumberOfArrays() - 1; a >= 0; --a) {
            vtkDataArray *d = data->GetArray(a);
            if (!d || d->GetDataType() != VTK_DOUBLE)
                continue;
            vtkNew<vtkFloatArray> f;
            f->DeepCopy(d);  // converts each value, keeps the name and component names
            data->RemoveArray(a);
            data->AddArray(f);
        }
    }
    return out;
}

} // namespace

bool writeVtkHdf(const ResultsSeries &series, const QString &path, QString *error)
{
    error->clear();
    if (series.points.empty()) {
        *error = QString("nothing to write to %1: the series has no time points").arg(path);
        return false;
    }
    const QFileInfo target(path);
    if (!target.absoluteDir().exists()) {
        *error = QString("cannot write %1: the folder does not exist").arg(path);
        return false;
    }

    // The blocks: the whole model, then each set under its kind.
    vtkNew<vtkDataAssembly> assembly;
    assembly->SetRootNodeName("Results");
    assembly->AddDataSetIndex(assembly->AddNode("Model"), 0);
    int nodeSets = -1, elementSets = -1;
    vtkUnstructuredGrid *first = series.points.front().grid;
    std::vector<SetMesh> meshes;
    for (const NamedSet &set : series.sets) {
        int &parent = set.nodes ? nodeSets : elementSets;
        if (parent < 0)
            parent = assembly->AddNode(set.nodes ? "NodeSets" : "ElementSets");
        const std::string name =
            vtkDataAssembly::MakeValidNodeName(set.name.toUtf8().constData());
        assembly->AddDataSetIndex(assembly->AddNode(name.c_str(), parent),
                                  unsigned(meshes.size() + 1));
        meshes.push_back(meshOf(set, first));
    }

    vtkNew<SeriesSource> source;
    for (const TimePoint &t : series.points) {
        // The provenance travels in every block of every time point.
        vtkNew<vtkStringArray> provenance;
        provenance->SetName("provenance");
        for (const QString &line : series.provenance)
            provenance->InsertNextValue(line.toStdString());
        provenance->InsertNextValue("stored: fields in single precision");
        vtkNew<vtkFieldData> fields;
        fields->DeepCopy(t.grid->GetFieldData());
        fields->AddArray(provenance);

        const vtkSmartPointer<vtkUnstructuredGrid> grid = singlePrecision(t.grid);
        auto model = vtkSmartPointer<vtkUnstructuredGrid>::New();
        model->ShallowCopy(grid);
        model->SetFieldData(fields);

        auto c = vtkSmartPointer<vtkPartitionedDataSetCollection>::New();
        c->SetNumberOfPartitionedDataSets(unsigned(meshes.size() + 1));
        c->SetPartition(0, 0, model);
        for (size_t k = 0; k < meshes.size(); ++k)
            c->SetPartition(unsigned(k + 1), 0, setGrid(meshes[k], grid, fields));
        c->SetDataAssembly(assembly);
        source->times.push_back(t.time);
        source->collections.push_back(c);
    }

    // Written beside the target and moved over it only once complete.
    const QString part = path + ".part";
    QString failure;
    vtkNew<vtkCallbackCommand> onError;
    onError->SetClientData(&failure);
    onError->SetCallback([](vtkObject *, unsigned long, void *client, void *call) {
        if (static_cast<QString *>(client)->isEmpty())
            *static_cast<QString *>(client) = QString::fromUtf8(static_cast<const char *>(call));
    });
    vtkNew<vtkHDFWriter> writer;
    writer->AddObserver(vtkCommand::ErrorEvent, onError);
    writer->SetInputConnection(source->GetOutputPort());
    writer->SetFileName(part.toUtf8().constData());
    writer->SetWriteAllTimeSteps(true);
    // Not compressed: compression (level 4, HDF5's own default) made the
    // writing of a 52-increment plastic run 23 s instead of 2.8, and each
    // reading of it 9 s instead of 1.6, to save 38 % of the size (123 MB
    // against 198; single-precision fields compress poorly). Measured
    // 2026-11-19, examples/crm661-tension; larger chunks were worse still.
    writer->SetCompressionLevel(0);
    const bool written = writer->Write() == 1 && failure.isEmpty();
    if (!written || (QFile::exists(path) && !QFile::remove(path)) || !QFile::rename(part, path)) {
        QFile::remove(part);
        *error = QString("cannot write %1%2").arg(path, failure.isEmpty() ? "" : ": " + failure.trimmed());
        return false;
    }
    return true;
}

} // namespace gf::core
