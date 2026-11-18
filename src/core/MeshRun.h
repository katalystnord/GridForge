#pragma once

// Meshing in a process of its own (`gf-study mesh-worker`), so that the
// mesher can neither take the window down nor keep it waiting (tenet 7).
// Netgen and OpenCASCADE run in the worker: a crash there ends the
// worker, and the window says so and goes on; Stop ends the worker at
// once, where Netgen's own stop flag is global and leaves a part-made mesh
// (docs/survey/netgen-review.md, items 2, 7, 8 and 10).
//
// The mesh comes back as one .vtu file (writeMeshFile): every coordinate
// and value exactly as made, the sets and the provenance in its field
// data, so the mesh the window reads is the mesh the worker made.

#include "core/ResultsSeries.h"

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>

namespace gf::core {

// A mesh (one time point) to `path` and back, exactly. The sets are kept
// as field data arrays "set nodes <name>" and "set elements <name>", the
// provenance as the string array "provenance"; reading takes them off the
// grid again. False, or an empty series, with *error set if it cannot.
bool writeMeshFile(const ResultsSeries &mesh, const QString &path, QString *error);
ResultsSeries readMeshFile(const QString &path, QString *error);

// The worker: gf-study beside the program running (an installed
// GridForge), else the one built with it (`built`, from the build tree);
// never both, as with the toolkits.
QString meshWorkerProgram(const QString &programDir, const QString &built);

// How a run of the worker ended, from what its process gave.
struct MeshEnd {
    enum class Kind { Made, Failed, Crashed, Stopped } kind = Kind::Failed;
    QString message;  // for Failed and Crashed, in words for the user
};

// The worker's end in words: made (exit 0), its own reason (exit 1, the
// last "gf-study: " line of its error output), stopped by the user, or
// ended by a crash. `file` is the geometry's file name.
MeshEnd meshEnd(QProcess::ExitStatus status, int exitCode, const QByteArray &errorOutput, bool stopped,
                const QString &file);

class MeshRun : public QObject
{
    Q_OBJECT
public:
    explicit MeshRun(QObject *parent = nullptr);
    ~MeshRun() override;

    // Meshes `geometry` with the mesh step's `parameters` (study.json's
    // "maxSize", "order", "throughWall") by running `program`, the mesh to
    // `out`. A run already going is stopped first, and its end not said.
    void start(const QString &program, const QString &geometry, const QJsonObject &parameters,
               const QString &out);
    // Ends the worker at once; finished() says Stopped.
    void stop();
    bool running() const { return m_process.state() != QProcess::NotRunning; }

signals:
    // On Made, the mesh is in the `out` given to start().
    void finished(const gf::core::MeshEnd &end);

private:
    QProcess m_process;
    QString m_file;
    bool m_stopping = false;
};

} // namespace gf::core
