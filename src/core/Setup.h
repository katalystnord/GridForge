#pragma once

// A study's set-up (docs/specs/study-v1.md, section 4): units, named
// groups of the geometry, materials, their assignment, supports and the
// analysis steps with their loads. Read from study.json's "setup" and
// checked: every name used is defined, every solid has one material.
// Solver-neutral; the CalculiX deck is written from it elsewhere.
//
// Version 1 here: supports (fixed, displacement, symmetry) and static
// steps with loads: a given displacement, a total force or a pressure on
// a group of faces, gravity on the solids (all, or a group's).

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <map>
#include <optional>
#include <vector>

namespace gf::core {

enum class Units { MillimetreNewtonSecond, MetreNewtonSecond };

struct Group {
    QString name;
    std::vector<int> faces;   // the geometry's face numbers, from 1
    std::vector<int> solids;  // the geometry's solid numbers, from 1
};

struct Material {
    QString name;
    QString card;     // a FreeCAD .FCMat in the study, or empty
    QString plastic;  // TenSense's *PLASTIC block in the study, or empty
    // Values given in the study itself, by FreeCAD's property names
    // ("YoungsModulus", "PoissonRatio", "Density"), in the study's units.
    // They fill or replace the card's, and are recorded as entered by hand.
    std::map<QString, double> values;
    // Where each value came from, for the deck's comments: "entered in the
    // study", or the card and the value as written there.
    std::map<QString, QString> source;
};

// A given movement of some directions (the others free), or none.
struct Movement {
    std::optional<double> x, y, z;
};

struct Support {
    QString group;
    bool fixed = false;
    bool symmetry = false;
    Movement displacement;  // used when neither fixed nor symmetry
};

struct Load {
    enum class Kind { Displacement, Force, Pressure, Gravity };
    Kind kind = Kind::Displacement;
    QString group;          // for gravity, empty: every solid
    Movement displacement;  // Displacement
    Movement vector;        // Force: the total, N; Gravity: the acceleration
    double pressure = 0;    // Pressure: normal, positive inward
};

// A step's increments (ccx's *STATIC data line): the first, the step's
// length in time, the smallest and largest allowed. None given: the
// solver's own.
struct Increments {
    std::optional<double> initial, period, min, max;
};

struct AnalysisStep {
    QString name;
    QString kind;  // "static"
    std::optional<bool> nonlinearGeometry;
    Increments increments;
    std::vector<Support> supports;
    std::vector<Load> loads;
    QStringList output;  // "displacement", "reaction", "stress", "strain", ...
    QStringList raw;     // keyword lines given by hand
    bool replace = false;
};

struct Setup {
    Units units = Units::MillimetreNewtonSecond;
    std::vector<Group> groups;
    std::vector<Material> materials;
    std::map<QString, QString> assign;  // group of solids -> material
    std::vector<Support> supports;      // from the start
    std::vector<AnalysisStep> steps;

    const Group *group(const QString &name) const;
    const Material *material(const QString &name) const;
};

// Reads and checks "setup". On a fault, an empty set-up and *error saying
// what and where ("steps[0].loads[1]: no group \"Top\"").
Setup readSetup(const QJsonObject &setup, QString *error);

} // namespace gf::core
