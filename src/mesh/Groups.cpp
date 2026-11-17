#include "mesh/Groups.h"

#include <set>

namespace gf::mesh {

namespace {

const gf::core::NamedSet *find(const gf::core::ResultsSeries &mesh, const QString &name, bool nodes)
{
    for (const gf::core::NamedSet &s : mesh.sets)
        if (s.name == name && s.nodes == nodes)
            return &s;
    return nullptr;
}

int count(const gf::core::ResultsSeries &mesh, const char *prefix)
{
    int n = 0;
    while (find(mesh, QString("%1%2").arg(prefix).arg(n + 1), true))
        ++n;
    return n;
}

} // namespace

bool addGroupSets(gf::core::ResultsSeries &mesh, const gf::core::Setup &setup, QString *error)
{
    const int faces = count(mesh, "Face"), solids = count(mesh, "Solid");
    std::vector<gf::core::NamedSet> added;
    for (const gf::core::Group &group : setup.groups) {
        std::set<vtkIdType> nodes, elements;
        for (int f : group.faces) {
            if (f > faces) {
                if (error)
                    *error = QString("group \"%1\": face %2; the geometry has %3").arg(group.name).arg(f).arg(faces);
                return false;
            }
            const gf::core::NamedSet *s = find(mesh, QString("Face%1").arg(f), true);
            nodes.insert(s->members.begin(), s->members.end());
        }
        for (int v : group.solids) {
            if (v > solids) {
                if (error)
                    *error = QString("group \"%1\": solid %2; the geometry has %3").arg(group.name).arg(v).arg(solids);
                return false;
            }
            const gf::core::NamedSet *n = find(mesh, QString("Solid%1").arg(v), true);
            const gf::core::NamedSet *e = find(mesh, QString("Solid%1").arg(v), false);
            nodes.insert(n->members.begin(), n->members.end());
            if (e)
                elements.insert(e->members.begin(), e->members.end());
        }
        added.push_back({group.name, true, {nodes.begin(), nodes.end()}});
        if (!group.solids.empty())
            added.push_back({group.name, false, {elements.begin(), elements.end()}});
    }
    mesh.sets.insert(mesh.sets.end(), added.begin(), added.end());
    if (error)
        error->clear();
    return true;
}

} // namespace gf::mesh
