#include "core/Job.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <vtkCellData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>

namespace gf::core {

namespace {

const char *setPrefix = "set ";

// A material's name as a file name: what a deck could take too.
QString fileName(QString name)
{
    for (QChar &c : name)
        if (!c.isLetterOrNumber() && c != '-' && c != '.')
            c = '_';
    return name;
}

} // namespace

bool writeSolveJob(const QString &folder, const QString &kind, const Setup &setup, const QJsonObject &setupJson,
                   const QString &studyDir, const ResultsSeries &mesh, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (mesh.points.empty() || !mesh.points.front().grid)
        return fail("no mesh for the job");
    const QDir job(folder);
    for (const char *sub : {"mesh", "materials", "results", "work"})
        if (!job.mkpath(sub))
            return fail(QString("cannot make %1").arg(job.filePath(sub)));

    // The set-up, resolved: values in, cards out, plastic blocks copied in.
    QJsonObject resolved = setupJson;
    QJsonObject materials;
    for (const Material &m : setup.materials) {
        QJsonObject values, source, out;
        for (const auto &[key, value] : m.values)
            values[key] = value;
        for (const auto &[key, from] : m.source)
            source[key] = from;
        out["values"] = values;
        out["source"] = source;
        if (!m.plastic.isEmpty()) {
            const QString inJob = "materials/" + fileName(m.name) + "-plastic.inp";
            QFile::remove(job.filePath(inJob));
            if (!QFile::copy(QDir(studyDir).filePath(m.plastic), job.filePath(inJob)))
                return fail(QString("material %1: cannot copy %2 into the job").arg(m.name, m.plastic));
            out["plastic"] = inJob;
        }
        materials[m.name] = out;
    }
    resolved["materials"] = materials;
    const QJsonObject document{{"format", "gridforge-job"}, {"version", "1.0"}, {"stage", "solve"},
                               {"kind", kind},              {"setup", resolved}};
    QSaveFile json(job.filePath("job.json"));
    if (!json.open(QIODevice::WriteOnly) || json.write(QJsonDocument(document).toJson()) < 0 || !json.commit())
        return fail("cannot write " + job.filePath("job.json"));

    // The mesh, its sets as arrays on a copy of the grid.
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    auto copy = vtkSmartPointer<vtkUnstructuredGrid>::New();
    copy->ShallowCopy(grid);
    for (const NamedSet &set : mesh.sets) {
        vtkNew<vtkUnsignedCharArray> flag;
        flag->SetName((setPrefix + set.name).toUtf8().constData());
        const vtkIdType n = set.nodes ? grid->GetNumberOfPoints() : grid->GetNumberOfCells();
        flag->SetNumberOfValues(n);
        flag->FillValue(0);
        for (vtkIdType m : set.members)
            flag->SetValue(m, 1);
        if (set.nodes)
            copy->GetPointData()->AddArray(flag);
        else
            copy->GetCellData()->AddArray(flag);
    }
    vtkNew<vtkXMLUnstructuredGridWriter> writer;
    writer->SetFileName(job.filePath("mesh/mesh.vtu").toUtf8().constData());
    writer->SetInputData(copy);
    if (writer->Write() != 1)
        return fail("cannot write " + job.filePath("mesh/mesh.vtu"));
    if (error)
        error->clear();
    return true;
}

SolveJob readSolveJob(const QString &folder, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return SolveJob{};
    };
    const QDir job(folder);
    QFile json(job.filePath("job.json"));
    if (!json.open(QIODevice::ReadOnly))
        return fail("no job.json in " + folder);
    const QJsonObject document = QJsonDocument::fromJson(json.readAll()).object();
    if (document["format"].toString() != "gridforge-job" || document["stage"].toString() != "solve")
        return fail(job.filePath("job.json") + ": not a solve job");
    const QString meshPath = job.filePath("mesh/mesh.vtu");
    if (!QFileInfo(meshPath).isFile())
        return fail("no mesh/mesh.vtu in " + folder);
    vtkNew<vtkXMLUnstructuredGridReader> reader;
    reader->SetFileName(meshPath.toUtf8().constData());
    reader->Update();
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->ShallowCopy(reader->GetOutput());
    if (grid->GetNumberOfCells() == 0)
        return fail(meshPath + ": no cells");

    SolveJob out;
    out.kind = document["kind"].toString();
    out.setup = document["setup"].toObject();
    // The sets back from their arrays, which leave the grid.
    for (bool nodes : {true, false}) {
        vtkFieldData *data = nodes ? static_cast<vtkFieldData *>(grid->GetPointData()) : grid->GetCellData();
        QStringList names;
        for (int a = 0; a < data->GetNumberOfArrays(); ++a)
            if (QString(data->GetArrayName(a)).startsWith(setPrefix))
                names << data->GetArrayName(a);
        for (const QString &name : names) {
            vtkDataArray *flag = data->GetArray(name.toUtf8().constData());
            NamedSet set{name.mid(QString(setPrefix).size()), nodes, {}};
            for (vtkIdType i = 0; i < flag->GetNumberOfTuples(); ++i)
                if (flag->GetTuple1(i) != 0)
                    set.members.push_back(i);
            out.mesh.sets.push_back(set);
            data->RemoveArray(name.toUtf8().constData());
        }
    }
    out.mesh.points.push_back({0.0, grid});
    if (error)
        error->clear();
    return out;
}

} // namespace gf::core
