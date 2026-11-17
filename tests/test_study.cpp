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
    f.open(QIODevice::ReadOnly);
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
    out.open(QIODevice::WriteOnly);
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
    CHECK(fields.size() == 2 && fields[0].name == "maxSize" && fields[0].label == "Largest element"
          && fields[0].unit == "mm" && fields[0].value == 3.0);
    CHECK(fields.size() == 2 && fields[1].name == "order" && fields[1].value == 2.0);
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
    CHECK(editStudy(folder, "material:Measured steel", {{"YoungsModulus", 205000.0}}, &error));
    s = readStudy(folder, &error);
    CHECK(s.setup.material("Measured steel")->values.at("YoungsModulus") == 205000);
}

CASE(an_edit_the_study_cannot_take_is_refused_and_the_file_kept)
{
    QTemporaryDir dir;
    const QString folder = copyDogbone(dir);
    QFile f(QDir(folder).filePath("study.json"));
    f.open(QIODevice::ReadOnly);
    const QByteArray before = f.readAll();
    f.close();
    QString error;
    CHECK(!editStudy(folder, "mesh:mesh", {{"maxSize", -1.0}}, &error));
    CHECK(error == "the largest element must be above 0");
    CHECK(!editStudy(folder, "mesh:mesh", {{"order", 3.0}}, &error));
    CHECK(error == "the order is 1 or 2");
    CHECK(!editStudy(folder, "load:0:9", {{"x", 1.0}}, &error));
    CHECK(error == "nothing to edit at load:0:9");
    CHECK(!editStudy(folder, "load:0:0", {{"w", 1.0}}, &error));
    CHECK(error == "load:0:0 has no w to edit");
    f.open(QIODevice::ReadOnly);
    CHECK(f.readAll() == before);
}

CASE(a_folder_without_a_study_is_said)
{
    QTemporaryDir dir;
    QString error;
    readStudy(dir.path(), &error);
    CHECK(error == "no study.json in " + dir.path());
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
