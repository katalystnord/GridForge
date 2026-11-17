#pragma once

// A solve stage's job folder (docs/specs/toolkits-v1.md, section 4): what
// the application hands a toolkit, and all the toolkit may read.
//
//   job/
//     job.json         {"format": "gridforge-job", "version": "1.0",
//                       "stage": "solve", "kind": ..., "setup": ...}; the
//                       set-up resolved: every material's values filled
//                       (and where each came from), its plastic block a
//                       file inside the job
//     mesh/mesh.vtu    the mesh, with "node id" and "element id", and each
//                       named set as an array "set <name>" (1 on its points
//                       or cells, 0 elsewhere)
//     materials/       <material>-plastic.inp, where a material has one
//     results/         the toolkit's results
//     work/            the toolkit's own files, kept
//
// So a job folder can be handed to a toolkit on another machine, or run
// again, with nothing outside it.

#include "core/ResultsSeries.h"
#include "core/Setup.h"

#include <QJsonObject>
#include <QString>

namespace gf::core {

struct SolveJob {
    QString kind;        // "structural.static"
    QJsonObject setup;   // as job.json holds it; readSetup reads it
    ResultsSeries mesh;  // one time point, its sets
};

// Writes the job folder for a set-up already filled from its cards
// (`setup`, with `setupJson` the study's own "setup" object it was read
// from, and `studyDir` where its files are). False with *error set.
bool writeSolveJob(const QString &folder, const QString &kind, const Setup &setup,
                   const QJsonObject &setupJson, const QString &studyDir, const ResultsSeries &mesh,
                   QString *error);

// Reads it back; an empty kind and *error set on a fault.
SolveJob readSolveJob(const QString &folder, QString *error);

} // namespace gf::core
