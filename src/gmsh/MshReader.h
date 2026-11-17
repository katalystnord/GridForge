#pragma once

// Gmsh meshes (.msh), read as the file says them. Written from the
// format's specification, the Gmsh manual's "MSH file format" (version
// 4.1) and "Node ordering" sections. Nothing here converts or renames:
// node order stays Gmsh's, tags stay the file's.
//
// Read, each proven by a fixture written by Gmsh 4.14.0: MSH 4.1 in ASCII
// and in binary; the sections $MeshFormat, $PhysicalNames, $Entities,
// $Nodes and $Elements; the element types a CalculiX deck can take (point,
// line2, line3, tri3, tri6, quad4, quad8, tet4, tet10, hex8, hex20, prism6,
// prism15). Refused by name: other format versions, other element types,
// nodes with parametric coordinates, partitioned and periodic meshes, and
// post-processing data. Sections the format leaves to the user ($Comments
// and the like) are passed over and listed.

#include <QString>
#include <QStringList>

#include <vector>

namespace gf::gmsh {

// Element types by the number the file uses (Gmsh manual, MSH file format).
// Only the types read so far are named; others are refused.
enum class MshElementType {
    Line2 = 1, Tri3 = 2, Quad4 = 3, Tet4 = 4, Hex8 = 5, Prism6 = 6, Line3 = 8, Tri6 = 9,
    Tet10 = 11, Point = 15, Quad8 = 16, Hex20 = 17, Prism15 = 18
};

// "4-node tetrahedron", "type 12" for one not read: for messages.
QString elementTypeName(int type);
int nodesPerElement(MshElementType type);
int dimensionOf(MshElementType type);

struct MshPhysicalName {
    int dimension = 0;
    int tag = 0;
    QString name;
};

// A model entity ($Entities): a point, curve, surface or volume, and the
// physical groups it belongs to.
struct MshEntity {
    int dimension = 0;
    int tag = 0;
    std::vector<int> physicalTags;
};

struct MshNode {
    long long tag = 0;
    double x = 0, y = 0, z = 0;
    int entityDimension = 0, entityTag = 0;
};

struct MshElement {
    long long tag = 0;
    MshElementType type = MshElementType::Point;
    int entityDimension = 0, entityTag = 0;
    std::vector<long long> nodes;  // node tags, in Gmsh's order
};

struct MshFile {
    QString version;  // "4.1"
    bool binary = false;
    std::vector<MshPhysicalName> physicalNames;
    std::vector<MshEntity> entities;
    std::vector<MshNode> nodes;
    std::vector<MshElement> elements;
    QStringList passedOver;  // sections read past, by name ("$Comments")
    QString error;           // empty when read; else where and why

    bool ok() const { return error.isEmpty(); }
    const MshEntity *entity(int dimension, int tag) const;
    // The physical group's name, or Gmsh's own for one without
    // ("PhysicalSurface77", as Gmsh's .inp export writes it).
    QString physicalName(int dimension, int tag) const;
};

MshFile readMsh(const QString &path);

} // namespace gf::gmsh
