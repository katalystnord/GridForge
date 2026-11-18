#pragma once

// What a face of the geometry was when a group was made on it (study spec
// 4.2): its kind, its area and its centre. On a new geometry a face that
// no longer matches is said, never guessed; matching faces across CAD
// changes in general is a known hard problem, and version 1 only notices.

#include <QJsonObject>
#include <QString>

#include <array>

namespace gf::core {

struct FaceSignature {
    QString kind;  // OpenCASCADE's kind of surface: "plane", "cylinder", ...
    double area = 0;
    std::array<double, 3> centre{0, 0, 0};  // the centre of its area
};

// The same face, to rounding: the same kind, the area within a millionth
// of it, the centre within a millionth of the face's size (the root of its
// area). A STEP written again rounds its numbers; a changed face moves
// further than that.
bool sameFace(const FaceSignature &was, const FaceSignature &now);

// A solid's: its volume and the centre of its volume.
struct SolidSignature {
    double volume = 0;
    std::array<double, 3> centre{0, 0, 0};
};

// The same solid, to rounding: the volume within a millionth of it, the
// centre within a millionth of its size (the cube root of its volume).
bool sameSolid(const SolidSignature &was, const SolidSignature &now);

// "a plane of 200 mm2 at (5, 10, 30)", lengths in `length`.
QString describeFace(const FaceSignature &s, const QString &length);

// As study.json keeps it: {"kind": "plane", "area": 200, "centre": [5, 10, 30]}.
QJsonObject faceSignatureJson(const FaceSignature &s);

} // namespace gf::core
