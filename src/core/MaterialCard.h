#pragma once

// FreeCAD 1.x material cards (.FCMat, YAML), the part a solver needs, in a
// study's units (study spec, section 4.3). Read by the models' UUIDs:
//
//   IsotropicLinearElastic  f6f9e48c-b116-4e82-ad7f-3659a9219c50  E, nu
//   LinearElastic           7b561d1d-fb9b-44f6-9da9-56a4f74d7536  E, nu
//   Density                 454661e5-265b-4320-8e6f-fcf6223ac3af  density
//
// (FreeCAD 1.1's own cards use the first, TenSense writes the second;
// both checked in FreeCAD 1.1.4's model files.) Values carry their unit
// ("200000 MPa", "7900 kg/m^3"); a unit not known here is refused by name,
// never guessed. A card's Inherits are not followed in version 1.

#include "core/Setup.h"

#include <QString>
#include <QStringList>

#include <map>

namespace gf::core {

struct CardValue {
    double value = 0;  // in the study's units
    QString asWritten; // "200000 MPa"
};

struct MaterialCard {
    QString name;  // General.Name
    std::map<QString, CardValue> values;  // "YoungsModulus", "PoissonRatio", "Density"
    QString error;
    bool ok() const { return error.isEmpty(); }
};

MaterialCard readMaterialCard(const QString &path, Units units);

// Fills each material's values the study does not give from its card
// (paths relative to studyDir), and records where each came from in
// Material::source. False with *error set if a card cannot be read.
bool fillFromCards(Setup &setup, const QString &studyDir, QString *error);

} // namespace gf::core
