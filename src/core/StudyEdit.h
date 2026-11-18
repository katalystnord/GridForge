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

// Adds a group of the geometry's faces (numbered from 1, as gf_mesh's
// Face<n> sets) to <folder>/study.json, named as given less spaces at
// either end. False with *error (and the file unchanged) without a name,
// with a name a group has already, or without faces.
bool addFaceGroup(const QString &folder, const QString &name, const std::vector<int> &faces, QString *error);

// Renames a group, and every support and load that names it. False with
// *error (the file unchanged) without a new name, with a name a group has
// already, or if no group has the old name.
bool renameGroup(const QString &folder, const QString &from, const QString &to, QString *error);

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
