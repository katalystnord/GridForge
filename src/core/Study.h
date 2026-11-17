#pragma once

// A study as study.json describes it (docs/specs/study-v1.md, sections 1
// and 2): a graph of steps, and the set-up its solve steps share.
//
//   {"format": "gridforge-study", "version": "1.0",
//    "steps": [
//      {"id": "part", "kind": "input", "file": "dogbone.step"},
//      {"id": "mesh", "kind": "mesh", "by": "gridforge",
//       "inputs": {"geometry": "part.file"}, "parameters": {"maxSize": 3, "order": 2}},
//      {"id": "solve", "kind": "solve", "by": {"toolkit": "calculix"},
//       "inputs": {"mesh": "mesh.mesh"}, "parameters": {"kind": "structural.static"}}],
//    "setup": {...}}
//
// Read and checked: ids unique, every link names a step that comes before
// it in the graph, no cycles, the set-up as readSetup checks it.

#include "core/Setup.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <map>
#include <vector>

namespace gf::core {

struct StudyStep {
    QString id;
    QString kind;     // "input", "mesh", "solve", "derive"
    QString by;       // "gridforge", or a toolkit's id
    QString file;     // an input step's file, relative to the study folder
    std::map<QString, QStringList> inputs;  // input name -> "step.output" links
    QJsonObject parameters;
};

struct Study {
    QString folder;  // absolute
    std::vector<StudyStep> steps;
    Setup setup;
    QJsonObject setupJson;  // the "setup" object as written, for a job folder

    const StudyStep *step(const QString &id) const;
    // The steps of one kind, in the file's order.
    std::vector<const StudyStep *> stepsOf(const QString &kind) const;
};

// Reads <folder>/study.json (or the file itself if a path to it is given).
// On a fault, an empty study and *error saying what and where.
Study readStudy(const QString &path, QString *error);

} // namespace gf::core
