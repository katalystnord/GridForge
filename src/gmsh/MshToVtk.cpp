#include "gmsh/MshToVtk.h"

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkStringArray.h>

#include <algorithm>
#include <climits>
#include <map>
#include <numeric>
#include <set>
#include <unordered_map>

namespace gf::gmsh {

namespace {

using gf::core::NamedSet;
using gf::core::ResultsSeries;
using gf::core::TimePoint;

// A VTK cell and, for each of its points in VTK's order, the index of the
// node in Gmsh's order. Each proven by a fixture (test_mesh_handoff).
struct CellMap {
    int cell = VTK_EMPTY_CELL;
    std::vector<int> order;
};

std::vector<int> identity(int n)
{
    std::vector<int> v(size_t(n), 0);
    std::iota(v.begin(), v.end(), 0);
    return v;
}

const CellMap &cellFor(MshElementType t)
{
    static const std::map<MshElementType, CellMap> maps = {
        {MshElementType::Point, {VTK_VERTEX, identity(1)}},
        {MshElementType::Line2, {VTK_LINE, identity(2)}},
        {MshElementType::Line3, {VTK_QUADRATIC_EDGE, identity(3)}},
        {MshElementType::Tri3, {VTK_TRIANGLE, identity(3)}},
        {MshElementType::Tri6, {VTK_QUADRATIC_TRIANGLE, identity(6)}},
        {MshElementType::Quad4, {VTK_QUAD, identity(4)}},
        {MshElementType::Quad8, {VTK_QUADRATIC_QUAD, identity(8)}},
        {MshElementType::Tet4, {VTK_TETRA, identity(4)}},
        // Gmsh puts the middle of edge 2-3 before that of 1-3; VTK after.
        {MshElementType::Tet10, {VTK_QUADRATIC_TETRA, {0, 1, 2, 3, 4, 5, 6, 7, 9, 8}}},
        {MshElementType::Hex8, {VTK_HEXAHEDRON, identity(8)}},
        // Gmsh numbers the edge middles by their lower corner (0-1, 0-3,
        // 0-4, 1-2, ...); VTK goes round the bottom, the top, then up.
        {MshElementType::Hex20,
         {VTK_QUADRATIC_HEXAHEDRON,
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 13, 9, 16, 18, 19, 17, 10, 12, 14, 15}}},
        // Gmsh's base triangle faces into the prism, VTK's linear wedge
        // out of it: the base and the top each turned over (1 and 2, 4 and
        // 5 swapped). VTK's quadratic wedge faces the other way from its
        // linear one, so the 15-node prism keeps Gmsh's corners and only
        // its edge middles move (VTK: round the base, the top, then up).
        {MshElementType::Prism6, {VTK_WEDGE, {0, 2, 1, 3, 5, 4}}},
        {MshElementType::Prism15,
         {VTK_QUADRATIC_WEDGE, {0, 1, 2, 3, 4, 5, 6, 9, 7, 12, 14, 13, 8, 10, 11}}},
    };
    static const CellMap none;
    const auto it = maps.find(t);
    return it == maps.end() ? none : it->second;
}

} // namespace

ResultsSeries toSeries(const MshFile &msh, const QString &source, QString *error)
{
    ResultsSeries series;
    if (!msh.ok()) {
        *error = msh.error;
        return series;
    }
    int dimension = -1;
    for (const MshElement &e : msh.elements)
        dimension = std::max(dimension, dimensionOf(e.type));
    if (dimension < 0) {
        *error = "the mesh has no elements";
        return series;
    }

    vtkNew<vtkUnstructuredGrid> grid;
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    vtkNew<vtkIntArray> nodeIds;
    nodeIds->SetName("node id");
    std::unordered_map<long long, vtkIdType> pointOf;
    for (const MshNode &n : msh.nodes) {
        if (n.tag > INT_MAX) {
            *error = QString("node %1: a tag above %2 is not converted yet").arg(n.tag).arg(INT_MAX);
            return series;
        }
        pointOf[n.tag] = points->InsertNextPoint(n.x, n.y, n.z);
        nodeIds->InsertNextValue(int(n.tag));
    }
    grid->SetPoints(points);
    grid->GetPointData()->AddArray(nodeIds);

    // The model's cells, and every element's points (for the sets).
    vtkNew<vtkIntArray> elementIds;
    elementIds->SetName("element id");
    std::vector<vtkIdType> cellOf(msh.elements.size(), -1);
    std::vector<std::vector<vtkIdType>> pointsOf(msh.elements.size());
    size_t lower = 0;
    for (size_t i = 0; i < msh.elements.size(); ++i) {
        const MshElement &e = msh.elements[i];
        for (long long tag : e.nodes) {
            const auto it = pointOf.find(tag);
            if (it == pointOf.end()) {
                *error = QString("element %1 refers to node %2, which the file does not hold")
                             .arg(e.tag)
                             .arg(tag);
                return series;
            }
            pointsOf[i].push_back(it->second);
        }
        if (dimensionOf(e.type) != dimension) {
            ++lower;
            continue;
        }
        const CellMap &map = cellFor(e.type);
        if (map.cell == VTK_EMPTY_CELL || e.tag > INT_MAX) {
            *error = QString("element %1 (%2) is not converted yet")
                         .arg(e.tag)
                         .arg(elementTypeName(int(e.type)));
            return series;
        }
        std::vector<vtkIdType> ids;
        for (int k : map.order)
            ids.push_back(pointsOf[i][size_t(k)]);
        cellOf[i] = grid->InsertNextCell(map.cell, vtkIdType(ids.size()), ids.data());
        elementIds->InsertNextValue(int(e.tag));
    }
    grid->GetCellData()->AddArray(elementIds);

    // Each physical group: the nodes of its elements; for a group of the
    // model's dimension, its cells as well. Highest dimension first, then
    // by tag.
    const auto order = [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    };
    std::map<std::pair<int, int>, std::vector<size_t>, decltype(order)> groups(order);
    std::map<std::pair<int, int>, const MshEntity *> entities;
    for (const MshEntity &ent : msh.entities)
        entities[{ent.dimension, ent.tag}] = &ent;
    for (size_t i = 0; i < msh.elements.size(); ++i) {
        const MshElement &e = msh.elements[i];
        const auto it = entities.find({e.entityDimension, e.entityTag});
        if (it == entities.end())
            continue;
        for (int p : it->second->physicalTags)
            groups[{e.entityDimension, p}].push_back(i);
    }
    int elementSets = 0;
    for (const auto &[key, members] : groups) {
        const QString name = msh.physicalName(key.first, key.second);
        std::set<vtkIdType> nodes;
        for (size_t i : members)
            nodes.insert(pointsOf[i].begin(), pointsOf[i].end());
        series.sets.push_back(NamedSet{name, true, {nodes.begin(), nodes.end()}});
        if (key.first == dimension) {
            NamedSet cells{name, false, {}};
            for (size_t i : members)
                cells.members.push_back(cellOf[i]);
            series.sets.push_back(std::move(cells));
            ++elementSets;
        }
    }
    // Node sets first, as the deck reader gives them.
    std::stable_sort(series.sets.begin(), series.sets.end(),
                     [](const NamedSet &a, const NamedSet &b) { return a.nodes && !b.nodes; });

    series.provenance << "mesher: Gmsh (the .msh does not name its version)"
                      << "source: " + source
                      << QString("format: MSH %1, %2").arg(msh.version, msh.binary ? "binary" : "ASCII");
    QString model = QString("model: the %1 elements of dimension %2")
                        .arg(grid->GetNumberOfCells())
                        .arg(dimension);
    if (lower > 0)
        model += QString("; %1 of lower dimension left out").arg(lower);
    series.provenance << model << "node order: Gmsh's, put into VTK's by GridForge";
    if (groups.empty())
        series.provenance << "sets: none, the file has no physical groups";
    else
        series.provenance << QString("sets: %1 physical group%2, as %3 node set%4 and %5 element set%6")
                                 .arg(groups.size())
                                 .arg(groups.size() == 1 ? "" : "s")
                                 .arg(groups.size())
                                 .arg(groups.size() == 1 ? "" : "s")
                                 .arg(elementSets)
                                 .arg(elementSets == 1 ? "" : "s");
    if (!msh.passedOver.isEmpty())
        series.provenance << "passed over: " + msh.passedOver.join(", ");

    vtkNew<vtkStringArray> provenance;
    provenance->SetName("provenance");
    for (const QString &line : series.provenance)
        provenance->InsertNextValue(line.toStdString());
    grid->GetFieldData()->AddArray(provenance);

    TimePoint t;
    t.time = 0;
    t.grid = grid;
    series.points.push_back(t);
    return series;
}

} // namespace gf::gmsh
