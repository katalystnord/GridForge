#include "core/ResultsSeries.h"

#include <QDir>
#include <QSaveFile>

#include <vtkFieldData.h>
#include <vtkNew.h>
#include <vtkStringArray.h>
#include <vtkXMLUnstructuredGridWriter.h>

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

} // namespace gf::core
