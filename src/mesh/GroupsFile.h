#pragma once

// Named groups from FreeCAD (docs/specs/groups-v1.md): the group file the
// macro writes beside its .brep, read against GridForge's own mesh of that
// geometry. Each face is found by its signature (kind, area, centre), each
// solid by its volume and centre, never by FreeCAD's number: GridForge
// joins touching solids, and joining renumbers faces.

#include "core/FaceSignature.h"
#include "core/ResultsSeries.h"
#include "core/Setup.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace gf::mesh {

struct GroupsFile {
    QString error;                      // the file as a whole refused; nothing found
    std::vector<gf::core::Group> groups;  // found: GridForge's numbers, the faces signed
    QStringList problems;               // each group not made, and why
};

// The group file at `path`, for the geometry file named `geometry`, its
// mesh `mesh`, in a study of `units`.
GroupsFile readGroupsFile(const QString &path, const QString &geometry, const gf::core::ResultsSeries &mesh,
                          gf::core::Units units);

// The faces (numbered from 1, in `faces`' order) that `signature` is.
std::vector<int> facesMatching(const gf::core::FaceSignature &signature,
                               const std::vector<gf::core::FaceSignature> &faces);

} // namespace gf::mesh
