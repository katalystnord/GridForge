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

} // namespace

QString inpNumber(double x)
{
    if (x == 0)
        return "0";
    // The shortest text that reads back exactly, if it fits in 20
    // characters; ccx 2.23 refuses a number with an exponent longer than
    // 21 (tried 2026-11-17). Else as many digits as 20 characters hold.
    char buffer[40];
    auto result = std::to_chars(buffer, buffer + sizeof buffer, x);
    for (int digits = 16; result.ptr - buffer > 20 && digits > 0; --digits)
        result = std::to_chars(buffer, buffer + sizeof buffer, x, std::chars_format::general, digits);
    return QString::fromLatin1(buffer, static_cast<int>(result.ptr - buffer));
}

namespace {

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

QString inpName(const QString &name)
{
    QString out = name;
    out.replace(' ', '_');
    out.replace(',', '_');
    return out;
}

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
    std::map<std::pair<bool, QString>, QString> written;  // (nodes, as written) -> as named
    for (const gf::core::NamedSet &set : mesh.sets) {
        if (set.name.isEmpty())
            return fail("a set has no name");
        const auto [at, fresh] = written.emplace(std::pair{set.nodes, inpName(set.name).toUpper()}, set.name);
        if (!fresh && at->second != set.name)  // ccx ignores case too
            return fail(QString("the sets \"%1\" and \"%2\" would both be written as %3")
                            .arg(std::min(at->second, set.name), std::max(at->second, set.name),
                                 inpName(set.name)));
    }

    QString text;
    text += QString("** written by %1 %2 (build %3)\n")
                .arg(gf::core::applicationName(), gf::core::applicationVersion(),
                     gf::core::applicationBuild());
    for (const QString &line : mesh.provenance)
        text += "** " + line + "\n";
    QStringList renamed;
    for (const gf::core::NamedSet &set : mesh.sets)
        if (inpName(set.name) != set.name && !renamed.contains(set.name))
            renamed << set.name;
    for (const QString &name : renamed)
        text += QString("** the set \"%1\" is written as %2: a CalculiX set name has no spaces or commas\n")
                    .arg(name, inpName(name));

    text += "*NODE, NSET=NALL\n";
    for (vtkIdType p = 0; p < grid->GetNumberOfPoints(); ++p) {
        double x[3];
        grid->GetPoint(p, x);
        text += QString("%1, %2, %3, %4\n").arg(idOf(nodeIds, p)).arg(inpNumber(x[0]), inpNumber(x[1]), inpNumber(x[2]));
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
        text += set.nodes ? QString("*NSET, NSET=%1\n").arg(inpName(set.name))
                          : QString("*ELSET, ELSET=%1\n").arg(inpName(set.name));
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
