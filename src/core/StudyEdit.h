#pragma once

// Editing a study from the window's Properties (keys as core::studyOutline
// gives them): what can be edited for an item, and the edit written to
// study.json, checked as readStudy checks it.
//
//   "mesh:<step>"        maxSize, order
//   "load:<s>:<k>"       a displacement or force: x, y, z (none: free / no
//                        component); a pressure: pressure; gravity: x, y, z
//   "support:<i>"        a displacement support: x, y, z
//   "material:<name>"    YoungsModulus, PoissonRatio, Density (given in the
//                        study; none: from the card)

#include "core/Study.h"

#include <QJsonObject>
#include <QString>

#include <array>

#include <map>
#include <optional>
#include <vector>

namespace gf::core {

struct EditableField {
    QString name;                 // as in study.json: "maxSize", "x", "YoungsModulus"
    QString label;                // for people: "Largest element", "x"
    QString unit;                 // in the study's units: "mm", "N", "MPa", "" for none
    std::optional<double> value;  // as the study gives it now; none: not given
    QString hint;                 // shown when none is given ("200000 MPa in Steel-Generic.FCMat", "free")
};

std::vector<EditableField> editableFields(const Study &study, const QString &key);

using Edit = std::map<QString, std::optional<double>>;  // name -> new value (none: remove)

// study.json's object with the edit made; an empty object and *error if
// the key or a name is not editable.
QJsonObject withEdit(const QJsonObject &study, const QString &key, const Edit &edit, QString *error);

// Reads <folder>/study.json, makes the edit, checks the result as readStudy
// does, and writes it back. False with *error (and the file unchanged) if
// any of that fails.
bool editStudy(const QString &folder, const QString &key, const Edit &edit, QString *error);

// A new study in `folder` (made if need be; refused over a study) for the
// geometry at `geometry` (a STEP or a FreeCAD .brep): the geometry copied
// into inputs/, with the group file beside it if there is one
// (<name>.groups.json, docs/specs/groups-v1.md); its mesh at `maxSize`,
// quadratic; a CalculiX solve; one static step, in mm, N, s.
bool createStudy(const QString &folder, const QString &geometry, double maxSize, QString *error);

// Adds a group of faces and/or solids, with its faces' signatures, as it
// is (a group from FreeCAD, docs/specs/groups-v1.md). False with *error
// and the file unchanged without faces or solids or with a name taken.
bool addGroup(const QString &folder, const Group &group, QString *error);

// Adds a material (study spec 4.3): its values, as the study's units,
// and, if `card` names a FreeCAD .FCMat, that card copied into inputs/
// (values given override the card's). False with *error, the file kept,
// for a name taken or a material with neither a card nor Young's modulus.
bool addMaterial(const QString &folder, const QString &name, const std::map<QString, double> &values,
                 const QString &card, QString *error);

// Gives a group of solids its material (study spec 4.4), in place of any
// it had. False with *error for no such group or material, or a group of
// no solids.
bool assignMaterial(const QString &folder, const QString &group, const QString &material, QString *error);

// Signs a group's faces again (study spec 4.2): `signatures` for its own
// faces replace those it had, as the faces are now. False with *error if
// there is no such group.
bool signGroup(const QString &folder, const QString &name, const std::map<int, FaceSignature> &signatures,
               QString *error);

// Adds a group of the geometry's faces (numbered from 1, as gf_mesh's
// Face<n> sets) to <folder>/study.json, named as given less spaces at
// either end. False with *error (and the file unchanged) without a name,
// with a name a group has already, or without faces. Each face's signature
// in `signatures` is kept with the group (study spec 4.2); others ignored.
bool addFaceGroup(const QString &folder, const QString &name, const std::vector<int> &faces, QString *error,
                  const std::map<int, FaceSignature> &signatures = {});

// Renames a group, and every support and load that names it. False with
// *error (the file unchanged) without a new name, with a name a group has
// already, or if no group has the old name.
bool renameGroup(const QString &folder, const QString &from, const QString &to, QString *error);

// A finer mesh on a group of faces: its largest element there, in the
// GridForge mesh step's "refine" (study spec 2.1); 0 takes it out.
bool setFinerMesh(const QString &folder, const QString &group, double size, QString *error);

// A comparison with a test (study spec 2.1, kind "compare"): the test's
// curve (TenSense's .vtp) copied into inputs/, an input step for it, and a
// compare step taking the first solve's results and the curve, with the
// group the force acts on, the extensometer's two points and the model's
// share of the test piece. *id is the compare step's.
bool addComparison(const QString &folder, const QString &curve, const QString &forceOn,
                   const std::array<double, 3> &from, const std::array<double, 3> &to, int modelShare, QString *id,
                   QString *error);
// Its group, gauge and share changed.
bool setComparison(const QString &folder, const QString &id, const QString &forceOn,
                   const std::array<double, 3> &from, const std::array<double, 3> &to, int modelShare,
                   QString *error);
// Taken out, with its input step (the curve's file stays in inputs/).
bool removeComparison(const QString &folder, const QString &id, QString *error);

// Deletes a group nothing uses. False with *error (the file unchanged),
// saying what uses it, if a support or load does.
bool deleteGroup(const QString &folder, const QString &name, QString *error);

// A support or load as core::entryOf writes it, added to <folder>/study.json
// at `place`: "supports" (from the start, in every step) or "step:<i>" (a
// fixed or symmetry support among the step's supports, anything else among
// its loads). Its key ("support:2", "support:0:1", "load:0:3"), or empty
// with *error (and the file unchanged) if the study cannot take it.
QString addItem(const QString &folder, const QString &place, const QJsonObject &entry, QString *error);

// The item at `key` replaced by `entry`, in its place: the same list, or
// the step's other one if the entry belongs there. Its key now, or empty.
QString replaceItem(const QString &folder, const QString &key, const QJsonObject &entry, QString *error);

// The item at `key` taken out.
bool removeItem(const QString &folder, const QString &key, QString *error);

// The entry at `key` in a study's "setup", as study.json holds it; empty
// if there is none.
QJsonObject itemEntry(const QJsonObject &setup, const QString &key);

} // namespace gf::core
