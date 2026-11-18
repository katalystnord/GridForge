// Named groups from FreeCAD (docs/specs/groups-v1.md): the macro's group
// file read against GridForge's own mesh of the geometry, each face found
// by its signature, not its number. The answers are the box's
// (fixtures/freecad/README.md): faces 1 to 6 are x = 0, x = 10, y = 0,
// y = 20, z = 0, z = 30, and its one solid is 6000 mm3 at (5, 10, 15).

#include "mesh/GroupsFile.h"
#include "mesh/Mesher.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdio>
#include <functional>
#include <vector>

using namespace gf;

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

const QString dir = QString(GF_FIXTURES) + "/freecad";

const core::ResultsSeries &box()
{
    static const core::ResultsSeries m = [] {
        QString error;
        return mesh::meshStep(dir + "/box.brep", mesh::Recipe{5, 1}, &error);
    }();
    return m;
}

// The fixture's group file with one change, written to a temporary file.
QString changed(QTemporaryDir &tmp, const std::function<void(QJsonObject &)> &change)
{
    QFile in(dir + "/box.groups.json");
    in.open(QIODevice::ReadOnly);
    QJsonObject o = QJsonDocument::fromJson(in.readAll()).object();
    change(o);
    const QString path = tmp.filePath("box.groups.json");
    QFile out(path);
    out.open(QIODevice::WriteOnly);
    out.write(QJsonDocument(o).toJson());
    return path;
}

const core::Group *named(const mesh::GroupsFile &g, const QString &name)
{
    for (const core::Group &group : g.groups)
        if (group.name == name)
            return &group;
    return nullptr;
}

} // namespace

CASE(the_boxs_groups_are_found_by_their_signatures)
{
    const mesh::GroupsFile g =
        mesh::readGroupsFile(dir + "/box.groups.json", "box.brep", box(), core::Units::MillimetreNewtonSecond);
    CHECK(g.error.isEmpty() && g.problems.isEmpty());
    CHECK(g.groups.size() == 3);
    const core::Group *back = named(g, "Back"), *ends = named(g, "Ends"), *part = named(g, "Steel part");
    CHECK(back && back->faces == std::vector<int>{1} && back->solids.empty());
    CHECK(ends && ends->faces == (std::vector<int>{5, 6}));
    CHECK(part && part->solids == std::vector<int>{1} && part->faces.empty());
    // Signed as the window signs a group: kept for the faces.
    CHECK(ends && ends->signatures.size() == 2 && ends->signatures.count(6)
          && ends->signatures.at(6).centre[2] == 30);
}

CASE(a_face_the_geometry_no_longer_has_is_said_and_its_group_not_made)
{
    QTemporaryDir tmp;
    const QString path = changed(tmp, [](QJsonObject &o) {
        // Face6 moved: the box made 35 tall since the groups were written.
        QJsonObject groups = o["groups"].toObject();
        QJsonObject ends = groups["Ends"].toObject();
        QJsonArray faces = ends["faces"].toArray();
        QJsonObject top = faces[1].toObject();
        top["centre"] = QJsonArray{5, 10, 35};
        faces[1] = top;
        ends["faces"] = faces;
        groups["Ends"] = ends;
        o["groups"] = groups;
    });
    const mesh::GroupsFile g = mesh::readGroupsFile(path, "box.brep", box(), core::Units::MillimetreNewtonSecond);
    CHECK(g.error.isEmpty());
    CHECK(!named(g, "Ends") && named(g, "Back") && named(g, "Steel part"));
    CHECK(g.problems == QStringList{"group \"Ends\" not made: FreeCAD's Face6 (a plane of 200 mm2 at (5, 10, 35)) "
                                    "is not a face of box.brep; pick the group's faces in GridForge"});
}

CASE(a_signature_two_faces_share_is_not_guessed_between)
{
    const core::FaceSignature plane{"plane", 200, {5, 10, 30}};
    const std::vector<core::FaceSignature> faces{
        {"plane", 600, {0, 10, 15}}, plane, {"plane", 200 * (1 + 1e-9), {5, 10, 30}}, {"cylinder", 200, {5, 10, 30}}};
    CHECK((mesh::facesMatching(plane, faces) == std::vector<int>{2, 3}));
    CHECK((mesh::facesMatching({"plane", 600, {0, 10, 15}}, faces) == std::vector<int>{1}));
    CHECK(mesh::facesMatching({"cone", 1, {0, 0, 0}}, faces).empty());
}

CASE(a_file_for_other_geometry_or_lengths_or_not_a_group_file_is_refused)
{
    QTemporaryDir tmp;
    const auto units = core::Units::MillimetreNewtonSecond;
    mesh::GroupsFile g = mesh::readGroupsFile(dir + "/box.groups.json", "bracket.brep", box(), units);
    CHECK(g.groups.empty() && g.error == "the groups are for box.brep, not bracket.brep");
    g = mesh::readGroupsFile(dir + "/box.groups.json", "box.brep", box(), core::Units::MetreNewtonSecond);
    CHECK(g.groups.empty() && g.error == "the groups are in mm, the study in m");
    g = mesh::readGroupsFile(changed(tmp, [](QJsonObject &o) { o["format"] = "something else"; }), "box.brep", box(),
                             units);
    CHECK(g.groups.empty() && g.error == "not a GridForge group file (its format is not gridforge-groups)");
    g = mesh::readGroupsFile(tmp.filePath("none.json"), "box.brep", box(), units);
    CHECK(g.groups.empty() && g.error.startsWith("cannot read "));
}

CASE(the_macros_own_files_are_read_as_written_in_freecad)
{
    // The .brep and group file GridForge's FreeCAD macro wrote, in FreeCAD
    // (run-macro-test.sh, before this test): read against GridForge's mesh
    // of that .brep, all three groups found.
    const QString out = qEnvironmentVariable("GF_FREECAD_OUT");
    if (out.isEmpty()) {
        std::printf("  (GF_FREECAD_OUT not set: the macro's files not read)\n");
        return;
    }
    QString error;
    const core::ResultsSeries m = mesh::meshStep(out + "/box.brep", mesh::Recipe{5, 1}, &error);
    CHECK(error.isEmpty());
    const mesh::GroupsFile g =
        mesh::readGroupsFile(out + "/box.groups.json", "box.brep", m, core::Units::MillimetreNewtonSecond);
    CHECK(g.error.isEmpty() && g.problems.isEmpty() && g.groups.size() == 3);
    const core::Group *ends = named(g, "Ends");
    CHECK(ends && ends->faces == (std::vector<int>{5, 6}));
}

int main()
{
    for (auto &[name, run] : cases()) {
        const int before = failures;
        run();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
