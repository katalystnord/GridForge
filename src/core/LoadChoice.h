#pragma once

// What the window's support and load form holds, and the study.json entry
// it stands for (study spec 4.6), both ways. Pure, so the form's rules are
// tested without a window. The form follows the precedents in
// docs/survey/loads-and-supports.md: a support holds every direction and
// releases each one; a movement or a force is one magnitude in a direction
// (an axis, a face's normal, an edge) or given by components.

#include "core/Direction.h"

#include <QJsonObject>
#include <QString>

namespace gf::core {

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

// One direction of a support, or of a movement given by components: free,
// held where it is, or moved by a value.
struct AxisState {
    enum class Kind { Free, Held, Value };
    Kind kind = Kind::Free;
    double value = 0;
};

struct LoadChoice {
    enum class Kind {
        Fixed,     // a support: held in x, y and z
        Axes,      // a support: each direction free, held or moved
        Symmetry,  // a support: no movement across the face
        Move,      // a step's given movement
        Force,     // a total force, spread over the faces by area
        Pressure,  // normal to the faces, positive inward
    };
    Kind kind = Kind::Fixed;
    QString group;
    Direction direction;     // Move, Force
    Vec3 unit;               // Move, Force along a Normal or an Edge: the outward normal
                             // or the edge's own way, as measured on the geometry
    double magnitude = 0;    // Move: a length; Force: N; Pressure: a stress
    bool othersHeld = false; // Move in a direction: the other directions held, else free
    AxisState axes[3];       // Axes, and Move given by components
    Vec3 components;         // Force given by components
};

// The entry study.json holds for the choice. Empty, with *error saying why,
// when it cannot be written: no group, nothing held, a force of nothing, a
// movement along a direction off x, y and z.
QJsonObject entryOf(const LoadChoice &choice, QString *error);

// The choice an entry stands for, to show it again. A bare displacement is
// a step's movement, or with `support` a support given per direction.
LoadChoice choiceOf(const QJsonObject &entry, bool support = false);

} // namespace gf::core
