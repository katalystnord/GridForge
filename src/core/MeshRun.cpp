#include "core/MeshRun.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include <vtkFieldData.h>
#include <vtkIdTypeArray.h>
#include <vtkNew.h>
#include <vtkStringArray.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>

namespace gf::core {

namespace {

const QString nodeSet = "set nodes ";
const QString elementSet = "set elements ";
const char *provenanceArray = "provenance";

} // namespace

bool writeMeshFile(const ResultsSeries &mesh, const QString &path, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return false;
    };
    if (mesh.points.size() != 1 || !mesh.points.front().grid)
        return fail("a mesh file holds one grid");
    // A shallow copy carries the sets and provenance; the mesh is left as
    // it was.
    vtkNew<vtkUnstructuredGrid> grid;
    grid->ShallowCopy(mesh.points.front().grid);
    vtkNew<vtkFieldData> field;
    field->ShallowCopy(mesh.points.front().grid->GetFieldData());
    grid->SetFieldData(field);
    for (const NamedSet &set : mesh.sets) {
        vtkNew<vtkIdTypeArray> members;
        members->SetName(((set.nodes ? nodeSet : elementSet) + set.name).toUtf8().constData());
        for (vtkIdType m : set.members)
            members->InsertNextValue(m);
        field->AddArray(members);
    }
    vtkNew<vtkStringArray> provenance;
    provenance->SetName(provenanceArray);
    for (const QString &line : mesh.provenance)
        provenance->InsertNextValue(line.toUtf8().constData());
    field->AddArray(provenance);

    // Raw binary, uncompressed: every bit as made. Written beside the
    // file and moved into place, so a reader never meets half a mesh.
    const QString part = path + ".part";
    vtkNew<vtkXMLUnstructuredGridWriter> writer;
    writer->SetFileName(part.toUtf8().constData());
    writer->SetInputData(grid);
    writer->SetDataModeToAppended();
    writer->EncodeAppendedDataOff();
    writer->SetCompressorTypeToNone();
    writer->SetHeaderTypeToUInt64();
    if (writer->Write() != 1) {
        QFile::remove(part);
        return fail(QString("cannot write the mesh to %1").arg(path));
    }
    QFile::remove(path);
    if (!QFile::rename(part, path)) {
        QFile::remove(part);
        return fail(QString("cannot write the mesh to %1").arg(path));
    }
    if (error)
        error->clear();
    return true;
}

ResultsSeries readMeshFile(const QString &path, QString *error)
{
    if (error)
        error->clear();
    vtkNew<vtkXMLUnstructuredGridReader> reader;
    if (!QFileInfo(path).isFile() || !reader->CanReadFile(path.toUtf8().constData())) {
        if (error)
            *error = QString("cannot read the mesh from %1").arg(path);
        return {};
    }
    reader->SetFileName(path.toUtf8().constData());
    reader->Update();
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->ShallowCopy(reader->GetOutput());
    if (grid->GetNumberOfPoints() == 0) {
        if (error)
            *error = QString("cannot read the mesh from %1").arg(path);
        return {};
    }
    ResultsSeries mesh;
    vtkFieldData *field = grid->GetFieldData();
    QStringList taken;
    for (int a = 0; a < field->GetNumberOfArrays(); ++a) {
        vtkAbstractArray *array = field->GetAbstractArray(a);
        const QString name = QString::fromUtf8(array->GetName() ? array->GetName() : "");
        if (name == provenanceArray) {
            auto *lines = vtkStringArray::SafeDownCast(array);
            for (vtkIdType i = 0; lines && i < lines->GetNumberOfValues(); ++i)
                mesh.provenance << QString::fromStdString(lines->GetValue(i));
            taken << name;
            continue;
        }
        const bool nodes = name.startsWith(nodeSet);
        if (!nodes && !name.startsWith(elementSet))
            continue;
        auto *members = vtkIdTypeArray::SafeDownCast(array);
        NamedSet set{name.mid((nodes ? nodeSet : elementSet).size()), nodes, {}};
        for (vtkIdType i = 0; members && i < members->GetNumberOfValues(); ++i)
            set.members.push_back(members->GetValue(i));
        mesh.sets.push_back(std::move(set));
        taken << name;
    }
    // A new field data without them: the reader's own is left alone.
    vtkNew<vtkFieldData> kept;
    for (int a = 0; a < field->GetNumberOfArrays(); ++a) {
        vtkAbstractArray *array = field->GetAbstractArray(a);
        if (!taken.contains(QString::fromUtf8(array->GetName() ? array->GetName() : "")))
            kept->AddArray(array);
    }
    grid->SetFieldData(kept);
    mesh.points.push_back({0, grid});
    return mesh;
}

QString meshWorkerProgram(const QString &programDir, const QString &built)
{
    const QFileInfo installed(programDir + "/gf-study");
    return installed.isFile() && installed.isExecutable() ? installed.filePath() : built;
}

MeshEnd meshEnd(QProcess::ExitStatus status, int exitCode, const QByteArray &errorOutput, bool stopped,
                const QString &file)
{
    if (stopped)
        return {MeshEnd::Kind::Stopped, {}};
    if (status == QProcess::NormalExit && exitCode == 0)
        return {MeshEnd::Kind::Made, {}};
    QStringList lines;
    for (const QByteArray &l : errorOutput.split('\n'))
        if (!l.trimmed().isEmpty())
            lines << QString::fromUtf8(l.trimmed());
    if (status == QProcess::NormalExit) {
        for (qsizetype i = lines.size() - 1; i >= 0; --i)
            if (lines[i].startsWith("gf-study: "))
                return {MeshEnd::Kind::Failed, lines[i].mid(10)};
        return {MeshEnd::Kind::Failed, QString("the mesher could not mesh %1, and gave no reason (exit code %2)")
                                           .arg(file).arg(exitCode)};
    }
    QString message = QString("the mesher stopped working while meshing %1 (it crashed; nothing of the "
                              "study is lost). This is a fault in the mesher")
                          .arg(file);
    if (!lines.isEmpty())
        message += "; its last words: " + lines.last();
    return {MeshEnd::Kind::Crashed, message};
}

MeshRun::MeshRun(QObject *parent) : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.setStandardOutputFile(QProcess::nullDevice());
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QByteArray said = m_process.readAllStandardError();
        const bool stopped = m_stopping;
        m_stopping = false;
        emit finished(meshEnd(status, code, said, stopped, m_file));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        m_stopping = false;
        emit finished({MeshEnd::Kind::Failed,
                       QString("the mesher (%1) could not be started: %2").arg(m_process.program(), m_process.errorString())});
    });
}

MeshRun::~MeshRun()
{
    if (running()) {
        m_process.blockSignals(true);
        m_process.kill();
        m_process.waitForFinished(2000);
    }
}

void MeshRun::start(const QString &program, const QString &geometry, const QJsonObject &parameters,
                    const QString &out)
{
    if (running()) {
        // The run replaced is not said: its end would be taken for this one's.
        m_process.blockSignals(true);
        m_process.kill();
        m_process.waitForFinished(2000);
        m_process.blockSignals(false);
    }
    m_stopping = false;
    m_file = QFileInfo(geometry).fileName();
    QFile::remove(out);
    m_process.start(program, {"mesh-worker", geometry, "--recipe",
                              QString::fromUtf8(QJsonDocument(parameters).toJson(QJsonDocument::Compact)), "-o", out});
}

void MeshRun::stop()
{
    if (!running())
        return;
    // Nothing of the worker's is worth keeping: its file is written whole
    // or not at all. So it is ended at once, not asked.
    m_stopping = true;
    m_process.kill();
}

} // namespace gf::core
