// A study read from study.json (study spec, sections 1 and 2): the dogbone
// example as written, then each fault refused, saying what and where.

#include "core/Study.h"
#include "core/StudyEdit.h"
#include "core/StudyOutline.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::core;

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

const QString dogbone = QString(GF_FIXTURES) + "/studies/dogbone";

QJsonObject example()
{
    QFile f(dogbone + "/study.json");
    CHECK(f.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(f.readAll()).object();
}

// The dogbone study with `change` made to it, written to a folder beside a
// copy of its geometry and materials, and read back: the error.
QString errorWith(const std::function<void(QJsonObject &)> &change)
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("materials");
    for (const char *f : {"dogbone.step", "materials/tensense-ro.FCMat", "materials/tensense-ro-plastic.inp"})
        QFile::copy(dogbone + "/" + f, QDir(dir.path()).filePath(f));
    QJsonObject o = example();
    change(o);
    QFile out(QDir(dir.path()).filePath("study.json"));
    CHECK(out.open(QIODevice::WriteOnly));
    out.write(QJsonDocument(o).toJson());
    out.close();
    QString error;
    readStudy(dir.path(), &error);
    return error;
}

QJsonObject &stepAt(QJsonObject &o, int i, QJsonArray &steps)
{
    steps = o["steps"].toArray();
    static QJsonObject s;
    s = steps[i].toObject();
    return s;
}

void setStep(QJsonObject &o, int i, const QJsonObject &s)
{
    QJsonArray steps = o["steps"].toArray();
    steps[i] = s;
    o["steps"] = steps;
}

} // namespace

CASE(the_dogbone_study_is_read_as_written)
{
    QString error;
    const Study s = readStudy(dogbone, &error);
    CHECK(error.isEmpty());
    if (!error.isEmpty())
        std::printf("  (%s)\n", qPrintable(error));
    CHECK(s.folder == QDir(dogbone).absolutePath());
    CHECK(s.steps.size() == 3);
    const StudyStep *part = s.step("part"), *mesh = s.step("mesh"), *solve = s.step("solve");
    CHECK(part && part->kind == "input" && part->file == "dogbone.step");
    CHECK(mesh && mesh->kind == "mesh" && mesh->by == "gridforge");
    CHECK(mesh && mesh->inputs.at("geometry") == QStringList({"part.file"}));
    CHECK(mesh && mesh->parameters["maxSize"].toDouble() == 3 && mesh->parameters["order"].toInt() == 2);
    CHECK(solve && solve->by == "calculix" && solve->inputs.at("mesh") == QStringList({"mesh.mesh"}));
    CHECK(s.stepsOf("solve").size() == 1 && s.stepsOf("solve")[0] == solve);
    CHECK(s.setup.material("Measured steel") && s.setup.group("Pulled grip"));
    CHECK(s.setupJson["units"].toString() == "mm-N-s");
}

CASE(the_file_itself_may_be_named)
{
    QString error;
    const Study s = readStudy(dogbone + "/study.json", &error);
    CHECK(error.isEmpty() && s.steps.size() == 3 && s.folder == QDir(dogbone).absolutePath());
}

CASE(each_fault_is_refused_saying_what_and_where)
{
    CHECK(errorWith([](QJsonObject &o) { o["format"] = "something-else"; })
          == "study.json: not a GridForge study (its format is \"something-else\")");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonArray st;
              QJsonObject s = stepAt(o, 2, st);
              s["id"] = "mesh";
              setStep(o, 2, s);
          })
          == "steps[2]: the id \"mesh\" is used twice");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonArray st;
              QJsonObject s = stepAt(o, 2, st);
              s["inputs"] = QJsonObject{{"mesh", "meshy.mesh"}};
              setStep(o, 2, s);
          })
          == "steps[2].inputs.mesh: no step \"meshy\"");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonArray st;
              QJsonObject s = stepAt(o, 2, st);
              s["inputs"] = QJsonObject{{"mesh", "part.mesh"}};
              setStep(o, 2, s);
          })
          == "steps[2].inputs.mesh: step \"part\" has no output \"mesh\" (it gives: file)");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonArray st;
              QJsonObject s = stepAt(o, 1, st);
              s["inputs"] = QJsonObject{{"geometry", "solve.results"}};
              setStep(o, 1, s);
          })
          == "steps[1].inputs.geometry: step \"solve\" comes after it; steps are listed in the "
             "order they can run");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonArray st;
              QJsonObject s = stepAt(o, 0, st);
              s["file"] = "dogbone.stp";
              setStep(o, 0, s);
          })
          == "steps[0]: no file dogbone.stp in the study's folder");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonArray st;
              QJsonObject s = stepAt(o, 1, st);
              s["kind"] = "bake";
              setStep(o, 1, s);
          })
          == "steps[1]: kind \"bake\" is not input, mesh, solve or derive");
    CHECK(errorWith([](QJsonObject &o) {
              QJsonObject setup = o["setup"].toObject();
              setup["units"] = "in-lbf-s";
              o["setup"] = setup;
          })
          == "setup.units: \"in-lbf-s\" is not one of mm-N-s, m-N-s");
}

// The outline as indented lines, "title | detail".
QStringList lines(const std::vector<OutlineItem> &items, int depth = 0)
{
    QStringList out;
    for (const OutlineItem &i : items) {
        out << QString(depth * 2, ' ') + i.title + (i.detail.isEmpty() ? QString() : " | " + i.detail);
        out << lines(i.children, depth + 1);
    }
    return out;
}

CASE(the_study_tree_shows_the_dogbone_as_the_drawing_does)
{
    QString error;
    const Study s = readStudy(dogbone, &error);
    const QStringList got = lines(studyOutline(s));
    const QStringList expected = {
        "Geometry | dogbone.step",
        "Mesh | GridForge (Netgen), largest element 3 mm, quadratic",
        "Materials",
        "  Measured steel | card tensense-ro.FCMat, plastic",
        "Groups",
        "  Dogbone | solid 1",
        "  Fixed grip | face 12",
        "  Pulled grip | face 6",
        "Supports | in every step",
        "  Fixed: Fixed grip",
        "Steps",
        "  Pull | static, large displacements",
        "    Displacement: Pulled grip | x 1.5, y 0, z 0 mm",
        "Jobs",
        "  solve | CalculiX, not run",
        "Results | none yet",
    };
    CHECK(got == expected);
    if (got != expected)
        for (const QString &l : got)
            std::printf("  got: %s\n", qPrintable(l));
}

CASE(every_kind_of_load_and_support_reads_in_the_studys_units)
{
    QString error;
    Study s = readStudy(dogbone, &error);
    QJsonObject o = s.setupJson;
    o["units"] = "m-N-s";
    o["supports"] = QJsonDocument::fromJson(R"([{"group": "Fixed grip", "symmetry": true},
        {"group": "Pulled grip", "displacement": {"y": 0}}])").array();
    QJsonArray steps = o["steps"].toArray();
    QJsonObject st = steps[0].toObject();
    st["nonlinear geometry"] = false;
    st["loads"] = QJsonDocument::fromJson(R"([{"group": "Pulled grip", "force": {"x": 1000}},
        {"group": "Pulled grip", "pressure": -2e6}, {"gravity": {"z": -9.81}}])").array();
    steps[0] = st;
    o["steps"] = steps;
    s.setup = readSetup(o, &error);
    CHECK(error.isEmpty());
    const QStringList got = lines(studyOutline(s));
    CHECK(got.contains("  Symmetry: Fixed grip"));
    CHECK(got.contains("  Displacement: Pulled grip | y 0 m"));
    CHECK(got.contains("  Pull | static"));
    CHECK(got.contains("    Force: Pulled grip | x 1000 N"));
    CHECK(got.contains("    Pressure: Pulled grip | -2000000 Pa"));
    CHECK(got.contains("    Gravity: every solid | z -9.81 m/s2"));
}

CASE(the_steps_table_shows_each_load_in_each_step_and_what_is_kept)
{
    // Pull 1.5 mm, then Hold (nothing new: the pull is kept), then Push
    // back to 0.5 with a force added. Supports at the start hold in every
    // step.
    QString error;
    Study s = readStudy(dogbone, &error);
    QJsonObject o = s.setupJson;
    QJsonArray steps = o["steps"].toArray();
    QJsonObject hold = steps[0].toObject(), push = steps[0].toObject();
    hold["name"] = "Hold";
    hold["loads"] = QJsonArray();
    push["name"] = "Push";
    push["loads"] = QJsonDocument::fromJson(R"([{"group": "Pulled grip", "force": {"y": 50}},
        {"group": "Pulled grip", "displacement": {"x": 0.5}}])").array();
    steps.append(hold);
    steps.append(push);
    o["steps"] = steps;
    s.setup = readSetup(o, &error);
    CHECK(error.isEmpty());
    const StepTable t = stepTable(s);
    CHECK(t.steps == QStringList({"Pull", "Hold", "Push"}));
    auto row = [&](const QString &title) -> const StepTable::Row * {
        for (const StepTable::Row &r : t.rows)
            if (r.title == title)
                return &r;
        return nullptr;
    };
    // Supports first, then loads in the order first met.
    CHECK(t.rows.size() == 3 && t.rows[0].title == "Fixed: Fixed grip");
    const StepTable::Row *fixed = row("Fixed: Fixed grip");
    CHECK(fixed && fixed->cells.size() == 3);
    for (int i = 0; fixed && i < 3; ++i)
        CHECK(fixed->cells[i].text == "fixed" && fixed->cells[i].key == "support:0" && !fixed->cells[i].kept);
    const StepTable::Row *pull = row("Displacement: Pulled grip");
    CHECK(pull && pull->cells.size() == 3);
    if (pull && pull->cells.size() == 3) {
        CHECK(pull->cells[0].text == "x 1.5, y 0, z 0 mm" && pull->cells[0].key == "load:0:0" && !pull->cells[0].kept);
        CHECK(pull->cells[1].text == "x 1.5, y 0, z 0 mm" && pull->cells[1].key == "load:0:0" && pull->cells[1].kept);
        CHECK(pull->cells[2].text == "x 0.5 mm" && pull->cells[2].key == "load:2:1" && !pull->cells[2].kept);
    }
    const StepTable::Row *force = row("Force: Pulled grip");
    CHECK(force && force->cells.size() == 3);
    if (force && force->cells.size() == 3) {
        CHECK(force->cells[0].text.isEmpty() && force->cells[0].key.isEmpty());
        CHECK(force->cells[1].text.isEmpty());
        CHECK(force->cells[2].text == "y 50 N" && force->cells[2].key == "load:2:0");
    }
}

CASE(each_run_of_a_step_gets_its_own_folder)
{
    // Every run's outputs are kept (David, 2026-11-17): runs/1, runs/2, ...
    QTemporaryDir dir;
    QFile::copy(dogbone + "/study.json", QDir(dir.path()).filePath("study.json"));
    Study s;
    s.folder = dir.path();
    const QString one = nextRunFolder(s, "solve"), two = nextRunFolder(s, "solve");
    CHECK(one == QDir(dir.path()).filePath("steps/solve/runs/1") && QDir(one).exists());
    CHECK(two == QDir(dir.path()).filePath("steps/solve/runs/2") && QDir(two).exists());
    QDir(dir.path()).mkpath("steps/solve/runs/9");
    CHECK(nextRunFolder(s, "solve").endsWith("runs/10"));
}

// A copy of the dogbone study in `dir`, its path.
QString copyDogbone(QTemporaryDir &dir)
{
    QDir(dir.path()).mkpath("materials");
    for (const char *f : {"study.json", "dogbone.step", "materials/tensense-ro.FCMat", "materials/tensense-ro-plastic.inp"})
        QFile::copy(dogbone + "/" + f, QDir(dir.path()).filePath(f));
    return dir.path();
}

CASE(what_can_be_edited_is_offered_with_its_value_and_unit)
{
    QString error;
    const Study s = readStudy(dogbone, &error);
    auto fields = editableFields(s, "mesh:mesh");
    CHECK(fields.size() == 3 && fields[0].name == "maxSize" && fields[0].label == "Largest element"
          && fields[0].unit == "mm" && fields[0].value == 3.0);
    CHECK(fields.size() == 3 && fields[1].name == "order" && fields[1].value == 2.0);
    // Not given in the study: blank, saying the default.
    CHECK(fields.size() == 3 && fields[2].name == "throughWall" && fields[2].label == "Elements through a wall"
          && !fields[2].value && fields[2].hint == "2");
    fields = editableFields(s, "load:0:0");  // Displacement: Pulled grip, x 1.5, y 0, z 0
    CHECK(fields.size() == 3 && fields[0].name == "x" && fields[0].value == 1.5 && fields[0].unit == "mm");
    CHECK(fields.size() == 3 && fields[2].name == "z" && fields[2].value == 0.0);
    fields = editableFields(s, "material:Measured steel");
    CHECK(fields.size() == 3 && fields[0].name == "YoungsModulus" && !fields[0].value
          && fields[0].hint == "from its card" && fields[0].unit == "MPa");
    CHECK(fields.size() == 3 && fields[1].name == "PoissonRatio" && fields[1].value == 0.3);
    CHECK(editableFields(s, "group:Dogbone").empty());  // not editable here (yet)
}

CASE(an_edit_is_written_to_the_study_and_read_back)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    CHECK(editStudy(folder, "load:0:0", {{"x", 2.0}, {"y", std::nullopt}}, &error) && error.isEmpty());
    Study s = readStudy(folder, &error);
    CHECK(error.isEmpty() && s.setup.steps[0].loads[0].displacement.x == 2.0);
    CHECK(!s.setup.steps[0].loads[0].displacement.y);  // removed: free
    CHECK(s.setup.steps[0].loads[0].displacement.z == 0.0);
    CHECK(editStudy(folder, "mesh:mesh", {{"maxSize", 2.5}}, &error));
    CHECK(readStudy(folder, &error).step("mesh")->parameters["maxSize"].toDouble() == 2.5);
    CHECK(editStudy(folder, "mesh:mesh", {{"throughWall", 3.0}}, &error));
    CHECK(readStudy(folder, &error).step("mesh")->parameters["throughWall"].toInt() == 3);
    CHECK(editStudy(folder, "material:Measured steel", {{"YoungsModulus", 205000.0}}, &error));
    s = readStudy(folder, &error);
    CHECK(s.setup.material("Measured steel")->values.at("YoungsModulus") == 205000);
}

CASE(an_edit_the_study_cannot_take_is_refused_and_the_file_kept)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QFile f(QDir(folder).filePath("study.json"));
    CHECK(f.open(QIODevice::ReadOnly));
    const QByteArray before = f.readAll();
    f.close();
    QString error;
    CHECK(!editStudy(folder, "mesh:mesh", {{"maxSize", -1.0}}, &error));
    CHECK(error == "the largest element must be above 0");
    CHECK(!editStudy(folder, "mesh:mesh", {{"order", 3.0}}, &error));
    CHECK(error == "the order is 1 or 2");
    CHECK(!editStudy(folder, "mesh:mesh", {{"throughWall", -1.0}}, &error));
    CHECK(error == "the elements through a wall are a whole number, 0 or more (0: none added)");
    CHECK(!editStudy(folder, "mesh:mesh", {{"throughWall", 1.5}}, &error));
    CHECK(error == "the elements through a wall are a whole number, 0 or more (0: none added)");
    CHECK(!editStudy(folder, "load:0:9", {{"x", 1.0}}, &error));
    CHECK(error == "nothing to edit at load:0:9");
    CHECK(!editStudy(folder, "load:0:0", {{"w", 1.0}}, &error));
    CHECK(error == "load:0:0 has no w to edit");
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == before);
}

CASE(picked_faces_become_a_named_group_in_the_study)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    // Picked in any order, a face twice: kept once each, in order.
    CHECK(addFaceGroup(folder, "  Grip faces ", {7, 3, 7}, &error) && error.isEmpty());
    const Study s = readStudy(folder, &error);
    const Group *g = s.setup.group("Grip faces");
    CHECK(error.isEmpty() && g && g->faces == std::vector<int>({3, 7}) && g->solids.empty());
    CHECK(s.setup.group("Fixed grip") && s.setup.group("Fixed grip")->faces == std::vector<int>({12}));
    CHECK(g && g->signatures.empty());  // none given: none written
    // With the faces' signatures, as the window measures them: kept for
    // the group's faces only.
    const FaceSignature seven{"cylinder", 70.28, {37.5, 7.3, 2}}, three{"plane", 80, {0, 0, 2}};
    CHECK(addFaceGroup(folder, "Signed", {7, 3}, &error, {{7, seven}, {3, three}, {9, three}}) && error.isEmpty());
    const Study t = readStudy(folder, &error);
    const Group *h = t.setup.group("Signed");
    CHECK(error.isEmpty() && h && h->signatures.size() == 2);
    if (h && h->signatures.size() == 2) {
        CHECK(h->signatures.at(7).kind == "cylinder" && h->signatures.at(7).area == 70.28
              && h->signatures.at(7).centre == seven.centre);
        CHECK(h->signatures.at(3).kind == "plane" && h->signatures.at(3).centre == three.centre);
    }
}

CASE(a_groups_faces_are_signed_again_as_they_are_now)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    const FaceSignature was{"plane", 80, {150, 0, 2}}, now{"plane", 80.5, {150.1, 0, 2}};
    CHECK(addFaceGroup(folder, "Grip", {6}, &error, {{6, was}}) && error.isEmpty());
    // Kept as they are now: the new signature replaces the old; one for a
    // face not in the group is not kept.
    CHECK(signGroup(folder, "Grip", {{6, now}, {9, was}}, &error) && error.isEmpty());
    const Study s = readStudy(folder, &error);
    const Group *g = s.setup.group("Grip");
    CHECK(error.isEmpty() && g && g->signatures.size() == 1 && g->signatures.count(6));
    if (g && g->signatures.count(6))
        CHECK(g->signatures.at(6).area == 80.5 && g->signatures.at(6).centre == now.centre);
    CHECK(!signGroup(folder, "Ghost", {{6, now}}, &error));
    CHECK(error == "there is no group named Ghost");
}

CASE(a_group_without_a_name_faces_or_a_name_of_its_own_is_refused)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QFile f(QDir(folder).filePath("study.json"));
    CHECK(f.open(QIODevice::ReadOnly));
    const QByteArray before = f.readAll();
    f.close();
    QString error;
    CHECK(!addFaceGroup(folder, " ", {1}, &error));
    CHECK(error == "a group needs a name");
    CHECK(!addFaceGroup(folder, "Fixed grip", {1}, &error));
    CHECK(error == "there is a group named Fixed grip already");
    CHECK(!addFaceGroup(folder, "New", {}, &error));
    CHECK(error == "no faces picked");
    CHECK(!addFaceGroup(folder, "New", {0}, &error));
    CHECK(error == "faces are numbered from 1");
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == before);
}

CASE(a_renamed_group_is_renamed_wherever_the_study_uses_it)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    CHECK(renameGroup(folder, "Pulled grip", " Moving grip ", &error) && error.isEmpty());
    const Study s = readStudy(folder, &error);
    CHECK(error.isEmpty() && !s.setup.group("Pulled grip") && s.setup.group("Moving grip"));
    CHECK(s.setup.group("Moving grip") && s.setup.group("Moving grip")->faces == std::vector<int>({6}));
    CHECK(!s.setup.steps.empty() && s.setup.steps[0].loads.size() == 1
          && s.setup.steps[0].loads[0].group == "Moving grip");
    CHECK(renameGroup(folder, "Fixed grip", "Held grip", &error));
    const Study t = readStudy(folder, &error);
    CHECK(t.setup.supports.size() == 1 && t.setup.supports[0].group == "Held grip");
    // The material assignment names groups too.
    CHECK(renameGroup(folder, "Dogbone", "Specimen", &error) && error.isEmpty());
    const Study u = readStudy(folder, &error);
    CHECK(error.isEmpty() && u.setup.assign.count("Specimen") && u.setup.assign.at("Specimen") == "Measured steel");
    CHECK(!u.setup.assign.count("Dogbone"));
}

CASE(a_rename_or_delete_the_study_cannot_take_is_refused_and_the_file_kept)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QFile f(QDir(folder).filePath("study.json"));
    CHECK(f.open(QIODevice::ReadOnly));
    const QByteArray before = f.readAll();
    f.close();
    QString error;
    CHECK(!renameGroup(folder, "Pulled grip", " ", &error));
    CHECK(error == "a group needs a name");
    CHECK(!renameGroup(folder, "Pulled grip", "Fixed grip", &error));
    CHECK(error == "there is a group named Fixed grip already");
    CHECK(!renameGroup(folder, "Nothing", "New", &error));
    CHECK(error == "there is no group named Nothing");
    // In use: said where, and kept.
    CHECK(!deleteGroup(folder, "Pulled grip", &error));
    CHECK(error == "Pulled grip is used by step Pull's displacement; take that out first");
    CHECK(!deleteGroup(folder, "Fixed grip", &error));
    CHECK(error == "Fixed grip is used by the supports; take that out first");
    CHECK(!deleteGroup(folder, "Dogbone", &error));
    CHECK(error == "Dogbone is used by the material assignment; take that out first");
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == before);
}

CASE(a_group_nothing_uses_is_deleted)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    CHECK(addFaceGroup(folder, "Spare", {3}, &error));
    CHECK(deleteGroup(folder, "Spare", &error) && error.isEmpty());
    CHECK(!readStudy(folder, &error).setup.group("Spare"));
}

CASE(a_folder_without_a_study_is_said)
{
    QTemporaryDir dir;
    QString error;
    readStudy(dir.path(), &error);
    CHECK(error == "no study.json in " + dir.path());
}

CASE(a_support_or_load_from_the_form_is_added_replaced_and_removed)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    // Bending: a move along z in the step Pull, a support from the start.
    const QJsonObject bend{{"group", "Pulled grip"}, {"displacement", QJsonObject{{"z", 2}}}, {"direction", "+z"}};
    CHECK(addItem(folder, "step:0", bend, &error) == "load:0:1" && error.isEmpty());
    CHECK(addItem(folder, "supports", QJsonObject{{"group", "Fixed grip"}, {"symmetry", true}}, &error) == "support:1");
    CHECK(addItem(folder, "step:0", QJsonObject{{"group", "Fixed grip"}, {"fixed", true}}, &error) == "support:0:0");
    Study s = readStudy(folder, &error);
    CHECK(error.isEmpty() && s.setup.steps[0].loads.size() == 2 && s.setup.supports.size() == 2);
    if (s.setup.steps[0].loads.size() != 2 || s.setup.steps[0].supports.size() != 1)
        return;
    CHECK(s.setup.steps[0].loads[1].displacement.z == 2.0 && s.setup.steps[0].loads[1].direction);
    CHECK(s.setup.steps[0].supports.size() == 1 && s.setup.steps[0].supports[0].fixed);
    CHECK(itemEntry(s.setupJson, "load:0:1") == bend);

    // Replaced by a force: the same place.
    const QJsonObject push{{"group", "Pulled grip"}, {"force", QJsonObject{{"z", 50}}}, {"direction", "+z"}};
    CHECK(replaceItem(folder, "load:0:1", push, &error) == "load:0:1");
    s = readStudy(folder, &error);
    CHECK(s.setup.steps[0].loads.size() == 2);
    if (s.setup.steps[0].loads.size() != 2)
        return;
    CHECK(s.setup.steps[0].loads[1].kind == Load::Kind::Force && s.setup.steps[0].loads[1].vector.z == 50.0);
    // A step's displacement replaced by a fixed support moves to the step's supports.
    CHECK(replaceItem(folder, "load:0:1", QJsonObject{{"group", "Pulled grip"}, {"fixed", true}}, &error)
          == "support:0:1");
    CHECK(removeItem(folder, "support:0:1", &error) && removeItem(folder, "support:0:0", &error));
    CHECK(removeItem(folder, "support:1", &error));
    s = readStudy(folder, &error);
    CHECK(s.setup.steps[0].loads.size() == 1 && s.setup.steps[0].supports.empty() && s.setup.supports.size() == 1);

    // Refused, the file kept.
    QFile f(QDir(folder).filePath("study.json"));
    CHECK(f.open(QIODevice::ReadOnly));
    const QByteArray before = f.readAll();
    f.close();
    CHECK(addItem(folder, "supports", push, &error).isEmpty());
    CHECK(error == "a force is given in a step, not among the supports");
    CHECK(addItem(folder, "step:3", bend, &error).isEmpty() && error == "there is no step 4");
    CHECK(addItem(folder, "step:0", QJsonObject{{"group", "Ghost"}, {"fixed", true}}, &error).isEmpty());
    CHECK(error == "setup.steps[0].supports[0]: no group \"Ghost\"");
    CHECK(!removeItem(folder, "load:0:5", &error) && error == "nothing at load:0:5");
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.readAll() == before);
}

CASE(an_edited_number_keeps_the_direction_only_while_it_still_holds)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    const QJsonObject push{{"group", "Pulled grip"}, {"force", QJsonObject{{"z", 50}}}, {"direction", "+z"}};
    CHECK(addItem(folder, "step:0", push, &error) == "load:0:1");
    auto entry = [&] { return itemEntry(readStudy(folder, &error).setupJson, "load:0:1"); };
    // A larger force, the same way: still +z.
    CHECK(editStudy(folder, "load:0:1", {{"z", 80.0}}, &error) && entry()["direction"] == "+z");
    // Turned round: -z.
    CHECK(editStudy(folder, "load:0:1", {{"z", -80.0}}, &error) && entry()["direction"] == "-z");
    // Given a second component: no longer along an axis, so given by components.
    CHECK(editStudy(folder, "load:0:1", {{"x", 10.0}}, &error) && !entry().contains("direction"));
    // A movement along +z whose z is taken out (freed) is no longer along z.
    const QJsonObject bend{{"group", "Pulled grip"}, {"displacement", QJsonObject{{"x", 0}, {"z", 2}}}, {"direction", "+z"}};
    CHECK(replaceItem(folder, "load:0:1", bend, &error) == "load:0:1");
    CHECK(editStudy(folder, "load:0:1", {{"z", std::nullopt}}, &error) && !entry().contains("direction"));
    // A face's normal survives a change of size, not of direction.
    const QJsonObject normal{{"group", "Pulled grip"}, {"force", QJsonObject{{"x", 3}, {"z", 4}}},
                             {"direction", "outward normal of face 6"}};
    CHECK(replaceItem(folder, "load:0:1", normal, &error) == "load:0:1");
    CHECK(editStudy(folder, "load:0:1", {{"x", -6.0}, {"z", -8.0}}, &error));
    CHECK(entry()["direction"] == "inward normal of face 6");
    CHECK(editStudy(folder, "load:0:1", {{"x", 1.0}}, &error) && !entry().contains("direction"));
}

CASE(a_load_given_in_a_direction_reads_in_words)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QString error;
    for (const QJsonObject &e :
         {QJsonObject{{"group", "Pulled grip"}, {"displacement", QJsonObject{{"z", 2}}}, {"direction", "+z"}},
          QJsonObject{{"group", "Pulled grip"}, {"displacement", QJsonObject{{"x", 0}, {"y", 0}, {"z", -2}}}, {"direction", "-z"}},
          QJsonObject{{"group", "Pulled grip"}, {"force", QJsonObject{{"x", 30}, {"z", 40}}}, {"direction", "inward normal of face 6"}},
          QJsonObject{{"group", "Fixed grip"}, {"force", QJsonObject{{"y", 50}}}, {"direction", "along edge 14"}}})
        CHECK(!addItem(folder, "step:0", e, &error).isEmpty());
    const Study s = readStudy(folder, &error);
    const std::vector<OutlineItem> tree = studyOutline(s);
    const OutlineItem *pull = nullptr;
    for (const OutlineItem &i : tree)
        if (i.kind == "steps" && !i.children.empty())
            pull = &i.children[0];
    CHECK(pull && pull->children.size() == 5);
    if (!pull || pull->children.size() != 5)
        return;
    CHECK(pull->children[0].detail == "x 1.5, y 0, z 0 mm");  // as typed: components
    CHECK(pull->children[1].detail == "2 mm along +z, others free");
    CHECK(pull->children[2].detail == "2 mm along -z, others held");
    CHECK(pull->children[3].detail == "50 N, inward normal of face 6");
    CHECK(pull->children[4].detail == "50 N along edge 14");
    // The steps table says the same.
    const StepTable t = stepTable(s);
    bool found = false;
    for (const StepTable::Row &r : t.rows)
        found = found || (r.title == "Force: Fixed grip" && r.cells[0].text == "50 N along edge 14");
    CHECK(found);
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
