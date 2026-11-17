// The .msh reader against meshes written by a released Gmsh, each with an
// answer known by hand (tests/fixtures/gmsh/README.md): a box 10 x 20 x 30
// mm, its faces x = 0 and x = 10 and its top as physical groups. The reader
// must give back exactly what the file holds, in Gmsh's own order.

#include "gmsh/MshReader.h"

#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

using namespace gf::gmsh;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
#endif

namespace {

int failures = 0;
std::vector<std::pair<const char *, std::function<void()>>> &cases()
{
    static std::vector<std::pair<const char *, std::function<void()>>> c;
    return c;
}
struct Register {
    Register(const char *n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
#define CASE(name)                                  \
    void name();                                    \
    static Register reg_##name(#name, name);        \
    void name()
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

QString fixture(const char *rel) { return QString(GF_FIXTURES) + "/gmsh/" + rel; }

MshFile read(const char *rel)
{
    MshFile f = readMsh(fixture(rel));
    if (!f.ok())
        std::printf("  (%s: %s)\n", rel, qPrintable(f.error));
    return f;
}

// How many elements of each type, by the file's type number.
std::map<int, int> countByType(const MshFile &f)
{
    std::map<int, int> n;
    for (const MshElement &e : f.elements)
        ++n[int(e.type)];
    return n;
}

std::unordered_map<long long, const MshNode *> nodesByTag(const MshFile &f)
{
    std::unordered_map<long long, const MshNode *> m;
    for (const MshNode &n : f.nodes)
        m[n.tag] = &n;
    return m;
}

// The elements on the entities of a physical group.
std::vector<const MshElement *> inGroup(const MshFile &f, int dimension, int tag)
{
    std::vector<const MshElement *> out;
    for (const MshElement &e : f.elements) {
        if (e.entityDimension != dimension)
            continue;
        const MshEntity *ent = f.entity(e.entityDimension, e.entityTag);
        for (int p : ent ? ent->physicalTags : std::vector<int>{})
            if (p == tag)
                out.push_back(&e);
    }
    return out;
}

QString writeTemp(QTemporaryDir &dir, const char *name, const QByteArray &data)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size())
        std::printf("  (cannot write %s)\n", qPrintable(path));
    return path;
}

QByteArray contents(const char *rel)
{
    QFile f(fixture(rel));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

CASE(a_structured_box_is_read_with_its_nodes_elements_and_groups)
{
    const MshFile f = read("box-hex8/box.msh");
    CHECK(f.ok());
    CHECK(f.version == "4.1" && !f.binary);
    // 2 x 3 x 4 hexahedra: 3 x 4 x 5 nodes.
    CHECK(f.nodes.size() == 60);
    CHECK(countByType(f) == (std::map<int, int>{{5, 24}, {3, 30}, {1, 2}, {15, 1}}));
    CHECK(f.physicalNames.size() == 5);
    const auto nodes = nodesByTag(f);
    CHECK(nodes.size() == 60);
    double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
    for (const MshNode &n : f.nodes) {
        const double x[3] = {n.x, n.y, n.z};
        for (int d = 0; d < 3; ++d) {
            lo[d] = std::min(lo[d], x[d]);
            hi[d] = std::max(hi[d], x[d]);
        }
    }
    CHECK(lo[0] == 0 && lo[1] == 0 && lo[2] == 0 && hi[0] == 10 && hi[1] == 20 && hi[2] == 30);
    // Every element's nodes are in the file.
    bool known = true;
    for (const MshElement &e : f.elements)
        for (long long t : e.nodes)
            known = known && nodes.count(t) == 1;
    CHECK(known);
}

CASE(physical_groups_are_found_through_the_entities)
{
    const MshFile f = read("box-hex8/box.msh");
    const auto nodes = nodesByTag(f);
    int fixedTag = 0, pulledTag = 0;
    for (const MshPhysicalName &p : f.physicalNames) {
        if (p.name == "Fixed")
            fixedTag = p.tag;
        if (p.name == "Pulled")
            pulledTag = p.tag;
    }
    CHECK(fixedTag > 0 && pulledTag > 0);
    const auto fixed = inGroup(f, 2, fixedTag), pulled = inGroup(f, 2, pulledTag),
               top = inGroup(f, 2, 77);
    CHECK(fixed.size() == 12 && pulled.size() == 12 && top.size() == 6);
    bool atX0 = true, atX10 = true, atTop = true;
    for (const MshElement *e : fixed)
        for (long long t : e->nodes)
            atX0 = atX0 && nodes.at(t)->x == 0;
    for (const MshElement *e : pulled)
        for (long long t : e->nodes)
            atX10 = atX10 && nodes.at(t)->x == 10;
    for (const MshElement *e : top)
        for (long long t : e->nodes)
            atTop = atTop && nodes.at(t)->z == 30;
    CHECK(atX0 && atX10 && atTop);
    CHECK(inGroup(f, 3, 1).size() == 24);
}

CASE(an_unnamed_group_takes_gmshs_own_name)
{
    const MshFile f = read("box-hex8/box.msh");
    CHECK(f.physicalName(2, 77) == "PhysicalSurface77");
    CHECK(f.physicalName(3, 1) == "Solid");
    // A name belongs to its dimension: tag 1 as a surface is not "Solid".
    CHECK(f.physicalName(2, 1) == "PhysicalSurface1");
    CHECK(f.physicalName(1, 5) == "PhysicalCurve5");
    CHECK(f.physicalName(0, 5) == "PhysicalPoint5");
    CHECK(f.physicalName(3, 5) == "PhysicalVolume5");
}

CASE(the_binary_file_holds_the_same_mesh_as_the_ascii_one)
{
    const MshFile a = read("box-hex8/box.msh"), b = read("box-hex8-binary/box.msh");
    CHECK(b.ok() && b.binary && b.version == "4.1");
    bool same = a.nodes.size() == b.nodes.size() && a.elements.size() == b.elements.size()
                && a.entities.size() == b.entities.size()
                && a.physicalNames.size() == b.physicalNames.size();
    // Gmsh writes ASCII coordinates to 16 significant digits (13.33333333333333)
    // and binary ones exactly (13.333333333333332): the same to that.
    const auto close = [](double x, double y) { return std::abs(x - y) <= 1e-15 * std::max(1.0, std::abs(x)); };
    for (size_t i = 0; same && i < a.nodes.size(); ++i)
        same = a.nodes[i].tag == b.nodes[i].tag && close(a.nodes[i].x, b.nodes[i].x)
               && close(a.nodes[i].y, b.nodes[i].y) && close(a.nodes[i].z, b.nodes[i].z)
               && a.nodes[i].entityDimension == b.nodes[i].entityDimension
               && a.nodes[i].entityTag == b.nodes[i].entityTag;
    for (size_t i = 0; same && i < a.elements.size(); ++i)
        same = a.elements[i].tag == b.elements[i].tag && a.elements[i].type == b.elements[i].type
               && a.elements[i].entityTag == b.elements[i].entityTag
               && a.elements[i].nodes == b.elements[i].nodes;
    for (size_t i = 0; same && i < a.entities.size(); ++i)
        same = a.entities[i].dimension == b.entities[i].dimension
               && a.entities[i].tag == b.entities[i].tag
               && a.entities[i].physicalTags == b.entities[i].physicalTags;
    CHECK(same);
}

CASE(each_element_type_is_read_with_its_node_count)
{
    struct Expect {
        const char *fixture;
        size_t nodes;
        std::map<int, int> types;
    };
    // Node counts by hand: hex20 has 60 corners and 40 + 45 + 48 edge
    // middles; prism15, 60 corners, 23 edges in each of 5 levels and 12
    // upright edges in each of 4 layers. The tetrahedra are not counted by
    // hand: their check is the box's volume, in the hand-off test.
    const Expect all[] = {
        {"box-hex20/box.msh", 193, {{17, 24}, {16, 30}, {8, 2}, {15, 1}}},
        {"box-prism6/box.msh", 60, {{6, 48}, {3, 24}, {2, 12}, {1, 2}, {15, 1}}},
        {"box-prism15/box.msh", 223, {{18, 48}, {16, 24}, {9, 12}, {8, 2}, {15, 1}}},
        {"box-tet4/box.msh", 294, {{4, 897}, {2, 306}, {1, 3}, {15, 1}}},
        {"box-tet10/box.msh", 1736, {{11, 897}, {9, 306}, {8, 3}, {15, 1}}},
    };
    for (const Expect &x : all) {
        const MshFile f = read(x.fixture);
        const bool ok = f.ok() && f.nodes.size() == x.nodes && countByType(f) == x.types;
        if (!ok)
            std::printf("  (%s: %zu nodes)\n", x.fixture, f.nodes.size());
        CHECK(ok);
        bool counts = true;
        for (const MshElement &e : f.elements)
            counts = counts && int(e.nodes.size()) == nodesPerElement(e.type);
        CHECK(counts);
    }
}

// Gmsh's middle nodes sit on the edges the manual's "Node ordering"
// drawings give them: the order a deck writer will translate from.
CASE(second_order_nodes_sit_where_gmshs_manual_draws_them)
{
    using Edges = std::vector<std::pair<int, int>>;
    const std::map<MshElementType, Edges> edges = {
        {MshElementType::Line3, {{0, 1}}},
        {MshElementType::Tri6, {{0, 1}, {1, 2}, {0, 2}}},
        {MshElementType::Quad8, {{0, 1}, {1, 2}, {2, 3}, {3, 0}}},
        {MshElementType::Tet10, {{0, 1}, {1, 2}, {0, 2}, {0, 3}, {2, 3}, {1, 3}}},
        {MshElementType::Hex20,
         {{0, 1}, {0, 3}, {0, 4}, {1, 2}, {1, 5}, {2, 3}, {2, 6}, {3, 7}, {4, 5}, {4, 7},
          {5, 6}, {6, 7}}},
        {MshElementType::Prism15,
         {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 4}, {2, 5}, {3, 4}, {3, 5}, {4, 5}}},
    };
    std::set<MshElementType> seen;
    for (const char *rel : {"box-hex20/box.msh", "box-prism15/box.msh", "box-tet10/box.msh"}) {
        const MshFile f = read(rel);
        const auto nodes = nodesByTag(f);
        int bad = 0;
        for (const MshElement &e : f.elements) {
            const auto it = edges.find(e.type);
            if (it == edges.end())
                continue;
            seen.insert(e.type);
            const size_t corners = e.nodes.size() - it->second.size();
            for (size_t k = 0; k < it->second.size(); ++k) {
                const MshNode *a = nodes.at(e.nodes[it->second[k].first]),
                              *b = nodes.at(e.nodes[it->second[k].second]),
                              *m = nodes.at(e.nodes[corners + k]);
                if (std::abs(m->x - (a->x + b->x) / 2) > 1e-9
                    || std::abs(m->y - (a->y + b->y) / 2) > 1e-9
                    || std::abs(m->z - (a->z + b->z) / 2) > 1e-9)
                    ++bad;
            }
        }
        if (bad)
            std::printf("  (%s: %d middle nodes off their edges)\n", rel, bad);
        CHECK(bad == 0);
    }
    CHECK(seen.size() == edges.size());
}

CASE(other_format_versions_are_refused_by_name)
{
    const MshFile f = readMsh(fixture("refused/msh22/box.msh"));
    CHECK(!f.ok());
    CHECK(f.error.contains("2.2") && f.error.contains("4.1"));
}

CASE(an_element_type_not_yet_read_is_refused_by_name)
{
    // Gmsh's complete second order: the faces' 9-node quadrangles come
    // before the 27-node hexahedra, and the first type not read is named.
    const MshFile f = readMsh(fixture("refused/hex27/box.msh"));
    CHECK(!f.ok());
    CHECK(f.error.contains("type 10"));
    CHECK(f.error.contains("9-node second order quadrangle"));
    CHECK(f.error.startsWith("line "));
}

CASE(a_missing_file_is_refused_by_name)
{
    const MshFile f = readMsh(fixture("no-such-folder/box.msh"));
    CHECK(!f.ok() && f.error.contains("no-such-folder/box.msh"));
}

CASE(sections_left_to_the_user_are_passed_over_and_listed)
{
    QTemporaryDir dir;
    QByteArray data = contents("box-hex8/box.msh");
    data.replace("$EndMeshFormat\n",
                 "$EndMeshFormat\n$Comments\nwritten by hand\n$Nodes in a comment\n"
                 "$EndComments\n");
    const MshFile f = readMsh(writeTemp(dir, "commented.msh", data));
    CHECK(f.ok());
    CHECK(f.passedOver == QStringList{"$Comments"});
    CHECK(f.nodes.size() == 60);
}

CASE(what_the_reader_does_not_handle_is_refused_by_name)
{
    QTemporaryDir dir;
    const QByteArray hex = contents("box-hex8/box.msh");
    struct Bad {
        const char *name;
        QByteArray data;
        const char *says;
    };
    // The hexahedra's block, said to lie on a surface.
    QByteArray misplaced = hex;
    misplaced.replace("\n3 1 5 24\n", "\n2 1 5 24\n");
    QByteArray parametric = hex;
    parametric.replace("$Nodes\n27 60 1 60\n0 1 0 1\n", "$Nodes\n27 60 1 60\n0 1 1 1\n");
    const Bad bad[] = {
        {"periodic.msh", hex + "$Periodic\n0\n$EndPeriodic\n", "$Periodic"},
        {"partitioned.msh",
         QByteArray(hex).replace("$Entities", "$PartitionedEntities\n0\n0\n0 0 0 0\n"
                                              "$EndPartitionedEntities\n$Entities"),
         "$PartitionedEntities"},
        {"data.msh", hex + "$NodeData\n0\n0\n0\n$EndNodeData\n", "$NodeData"},
        {"parametric.msh", parametric, "parametric"},
        {"cut.msh", hex.left(hex.indexOf("$EndNodes")), "$Nodes"},
        {"misplaced.msh", misplaced, "8-node hexahedron elements on an entity of dimension 2"},
        {"empty.msh", QByteArray(), "$MeshFormat"},
    };
    for (const Bad &b : bad) {
        const MshFile f = readMsh(writeTemp(dir, b.name, b.data));
        const bool said = !f.ok() && f.error.contains(b.says);
        if (!said)
            std::printf("  (%s: \"%s\")\n", b.name, qPrintable(f.error));
        CHECK(said);
    }
}

CASE(a_cut_binary_file_is_refused_where_it_ends)
{
    QTemporaryDir dir;
    const QByteArray bin = contents("box-hex8-binary/box.msh");
    const qsizetype elements = bin.indexOf("$Elements");
    CHECK(elements > 0);
    const MshFile f = readMsh(writeTemp(dir, "cut.msh", bin.left(elements + 40)));
    CHECK(!f.ok());
    CHECK(f.error.contains("$Elements") && f.error.contains("byte"));
}

int main()
{
    for (const auto &[name, f] : cases()) {
        const int before = failures;
        f();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%zu cases, %d checks failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
