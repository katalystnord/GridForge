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

} // namespace gf::core
