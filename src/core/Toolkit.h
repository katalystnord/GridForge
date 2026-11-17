#pragma once

// Toolkits (docs/specs/toolkits-v1.md, DRAFT): what does a stage of a study
// (mesh, solve, derive). Each is a folder with a toolkit.json and a command;
// the application talks to it through a job folder only, whatever its
// language or licence. The shipped CalculiX and OpenFOAM toolkits use the
// same door as anyone else's.

#include <QString>
#include <QStringList>

#include <vector>

namespace gf::core {

struct ToolkitStage {
    QString stage;        // "mesh", "solve", "derive"
    QStringList kinds;    // e.g. "structural.static", "fluid.incompressible.steady"
    QStringList features; // e.g. "plasticity", "contact"
    QString trust;        // "solved", "surrogate", "derived"
};

// A program the toolkit needs: one name, or any one of several.
struct ToolkitRequirement {
    QStringList oneOf;
};

struct Toolkit {
    QString folder;  // absolute
    QString id, name, toolkitVersion, licence, homepage;
    std::vector<ToolkitStage> stages;
    QStringList command;  // "{job}" stands for the job folder
    std::vector<ToolkitRequirement> requirements;  // "requires" in the manifest
    QStringList notices;  // to be shown wherever the toolkit is described
};

// Whether a toolkit can do a stage of a kind with the features a study
// needs, and if not, why, in words.
struct Offer {
    const Toolkit *toolkit = nullptr;
    bool can = false;
    QString why;    // empty when it can
    QString trust;  // of the stage that can
};

struct ToolkitSet {
    std::vector<Toolkit> toolkits;
    QStringList problems;  // manifests refused, each with its reason
    QStringList notes;     // e.g. one toolkit overriding another of the same id

    const Toolkit *find(const QString &id) const;
    // Every toolkit with that stage at all, each saying whether it can.
    std::vector<Offer> offers(const QString &stage, const QString &kind,
                              const QStringList &features) const;
};

// The toolkits in each folder's subfolders, in order: a later folder's
// toolkit replaces an earlier one with the same id (shipped, then the
// user's, then a study's own).
ToolkitSet findToolkits(const QStringList &folders);

// One toolkit's manifest, or an empty id and the reason in *error.
Toolkit readToolkit(const QString &folder, QString *error);

struct Availability {
    bool ready = false;
    QStringList missing;  // programs not found: the command's, or a requirement
};
// Whether the toolkit's programs are there: its command's program (in its
// folder or on PATH) and each requirement (where GF_<PROGRAM> names it,
// e.g. GF_CCX, else on PATH).
Availability availability(const Toolkit &t);

// The first toolkit that can do a stage of a kind and whose programs are
// there; null, and *why naming each candidate's reason, if none.
const Toolkit *readyToolkit(const ToolkitSet &set, const QString &stage, const QString &kind, QString *why);

// The command to run for a job folder.
QStringList commandFor(const Toolkit &t, const QString &jobFolder);

} // namespace gf::core
