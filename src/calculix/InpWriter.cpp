#include "calculix/InpWriter.h"

#include "core/ApplicationInfo.h"

#include <QSaveFile>

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>

#include <charconv>
#include <map>

namespace gf::calculix {

namespace {

// The shortest text that reads back as the same double; never "-0".
QString number(double x)
{
    if (x == 0)
        return "0";
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, x);
    return QString::fromLatin1(buffer, static_cast<int>(result.ptr - buffer));
}

QString cellName(int type)
{
    static const std::map<int, QString> names = {
        {VTK_VERTEX, "vertex"},       {VTK_LINE, "line"},
        {VTK_TRIANGLE, "triangle"},   {VTK_QUAD, "quadrilateral"},
        {VTK_HEXAHEDRON, "hexahedron"}, {VTK_WEDGE, "wedge"},
        {VTK_PYRAMID, "pyramid"},     {VTK_QUADRATIC_HEXAHEDRON, "quadratic hexahedron"},
        {VTK_QUADRATIC_WEDGE, "quadratic wedge"}, {VTK_QUADRATIC_TRIANGLE, "quadratic triangle"},
        {VTK_QUADRATIC_QUAD, "quadratic quadrilateral"}, {VTK_QUADRATIC_EDGE, "quadratic line"},
    };
    const auto it = names.find(type);
    return it != names.end() ? it->second : QString("VTK cell type %1").arg(type);
}

// The id of each point or cell: the grid's array of that name, or the
// place counted from 1.
long long idOf(vtkDataArray *ids, vtkIdType at)
{
    return ids ? static_cast<long long>(ids->GetTuple1(at)) : static_cast<long long>(at + 1);
}

} // namespace

QString meshInp(const gf::core::ResultsSeries &mesh, QString *error)
{
    auto fail = [&](const QString &why) {
        if (error)
            *error = why;
        return QString();
    };
    if (mesh.points.empty() || !mesh.points.front().grid)
        return fail("no mesh to write");
    vtkUnstructuredGrid *grid = mesh.points.front().grid;
    vtkDataArray *nodeIds = grid->GetPointData()->GetArray("node id");
    vtkDataArray *elementIds = grid->GetCellData()->GetArray("element id");

    for (vtkIdType c = 0; c < grid->GetNumberOfCells(); ++c) {
        const int type = grid->GetCellType(c);
        if (type != VTK_TETRA && type != VTK_QUADRATIC_TETRA)
            return fail(QString("a %1 is not written to a CalculiX deck yet; tetrahedra are").arg(cellName(type)));
    }
    for (const gf::core::NamedSet &set : mesh.sets)
        if (set.name.isEmpty() || set.name.contains(' ') || set.name.contains(','))
            return fail(QString("the set name \"%1\" has a space or a comma; a CalculiX set name cannot")
                            .arg(set.name));

    QString text;
    text += QString("** written by %1 %2 (build %3)\n")
                .arg(gf::core::applicationName(), gf::core::applicationVersion(),
                     gf::core::applicationBuild());
    for (const QString &line : mesh.provenance)
        text += "** " + line + "\n";

    text += "*NODE, NSET=NALL\n";
    for (vtkIdType p = 0; p < grid->GetNumberOfPoints(); ++p) {
        double x[3];
        grid->GetPoint(p, x);
        text += QString("%1, %2, %3, %4\n").arg(idOf(nodeIds, p)).arg(number(x[0]), number(x[1]), number(x[2]));
    }

    // One *ELEMENT block per type, in the order the types first appear.
    // VTK's node order is ccx's for C3D4 and C3D10.
    for (const auto &[type, keyword] : {std::pair{VTK_TETRA, "C3D4"}, std::pair{VTK_QUADRATIC_TETRA, "C3D10"}}) {
        QString block;
        for (vtkIdType c = 0; c < grid->GetNumberOfCells(); ++c) {
            if (grid->GetCellType(c) != type)
                continue;
            vtkIdType n;
            const vtkIdType *pts;
            grid->GetCellPoints(c, n, pts);
            block += QString::number(idOf(elementIds, c));
            for (vtkIdType k = 0; k < n; ++k)
                block += ", " + QString::number(idOf(nodeIds, pts[k]));
            block += "\n";
        }
        if (!block.isEmpty())
            text += QString("*ELEMENT, TYPE=%1, ELSET=EALL\n").arg(keyword) + block;
    }

    for (const gf::core::NamedSet &set : mesh.sets) {
        text += set.nodes ? QString("*NSET, NSET=%1\n").arg(set.name) : QString("*ELSET, ELSET=%1\n").arg(set.name);
        for (size_t k = 0; k < set.members.size(); ++k) {
            text += QString::number(idOf(set.nodes ? nodeIds : elementIds, set.members[k]));
            text += (k + 1) % 10 == 0 || k + 1 == set.members.size() ? "\n" : ", ";
        }
    }
    return text;
}

bool writeMeshInp(const gf::core::ResultsSeries &mesh, const QString &path, QString *error)
{
    const QString text = meshInp(mesh, error);
    if (text.isEmpty())
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        if (error)
            *error = QString("cannot write %1").arg(path);
        return false;
    }
    return true;
}

} // namespace gf::calculix
