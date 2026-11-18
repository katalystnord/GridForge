// Meshing in a process of its own (core/MeshRun): the mesh handed back
// exactly through its file, the worker's reasons passed on, a crash said
// without taking the caller down, and Stop ending the worker at once. The
// worker is the gf-study built with these tests; stand-ins written here
// play a mesher that crashes and one that never ends.

#include "core/MeshRun.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkStringArray.h>

#include <cstdio>
#include <functional>
#include <optional>
#include <vector>

using namespace gf::core;

#ifndef GF_FIXTURES
#error "GF_FIXTURES must point at tests/fixtures"
#endif
#ifndef GF_STUDY
#error "GF_STUDY must point at the gf-study built"
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

// A stand-in mesher: a shell script doing `body`.
QString standIn(const QTemporaryDir &dir, const char *name, const char *body)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(QByteArray("#!/bin/sh\n") + body + "\n");
    f.close();
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

// Runs the worker to its end (at most `seconds`), `during` called once it
// has started; the end said, or nothing if it did not end in time.
std::optional<MeshEnd> run(MeshRun &meshing, const QString &program, const QString &geometry,
                           const QJsonObject &parameters, const QString &out, int seconds = 120,
                           std::function<void()> during = {})
{
    std::optional<MeshEnd> end;
    QEventLoop loop;
    QObject::connect(&meshing, &MeshRun::finished, &loop, [&](const MeshEnd &e) {
        end = e;
        loop.quit();
    });
    QTimer::singleShot(seconds * 1000, &loop, &QEventLoop::quit);
    meshing.start(program, geometry, parameters, out);
    if (during)
        QTimer::singleShot(300, &loop, during);
    loop.exec();
    QObject::disconnect(&meshing, nullptr, &loop, nullptr);
    return end;
}

CASE(a_mesh_through_its_file_is_the_same_mesh)
{
    ResultsSeries mesh;
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkNew<vtkPoints> points;
    points->SetDataTypeToDouble();
    // Values no decimal text keeps: the file must hold every bit.
    points->InsertNextPoint(0.1, 1.0 / 3.0, 2e-17);
    points->InsertNextPoint(10.000000000000002, 0, 0);
    points->InsertNextPoint(0, 7.1, 0);
    points->InsertNextPoint(0, 0, 3.3333333333333335);
    grid->SetPoints(points);
    const vtkIdType tet[4] = {0, 1, 2, 3};
    grid->InsertNextCell(VTK_TETRA, 4, tet);
    vtkNew<vtkIntArray> ids;
    ids->SetName("node id");
    for (int i = 1; i <= 4; ++i)
        ids->InsertNextValue(i);
    grid->GetPointData()->AddArray(ids);
    vtkNew<vtkIntArray> solid;
    solid->SetName("solid");
    solid->InsertNextValue(1);
    grid->GetCellData()->AddArray(solid);
    vtkNew<vtkDoubleArray> area;
    area->SetName("face area");
    area->InsertNextValue(0.30000000000000004);
    area->InsertNextValue(1e300);
    grid->GetFieldData()->AddArray(area);
    vtkNew<vtkStringArray> kind;
    kind->SetName("face kind");
    kind->InsertNextValue("plane");
    kind->InsertNextValue("B-spline surface");
    grid->GetFieldData()->AddArray(kind);
    mesh.points.push_back({0, grid});
    mesh.sets = {{"Solid1", false, {0}}, {"Solid1", true, {0, 1, 2, 3}}, {"Face 2", true, {1, 3}}};
    mesh.provenance = {"mesher: Netgen 6.2", "recipe: largest element 3 mm, order 1"};

    QTemporaryDir dir;
    QString error;
    const QString path = dir.filePath("mesh.vtu");
    CHECK(writeMeshFile(mesh, path, &error));
    CHECK(error.isEmpty());
    const ResultsSeries back = readMeshFile(path, &error);
    CHECK(error.isEmpty());
    CHECK(back.points.size() == 1);
    if (back.points.size() != 1)
        return;
    vtkUnstructuredGrid *g = back.points.front().grid;
    CHECK(g->GetNumberOfPoints() == 4 && g->GetNumberOfCells() == 1);
    for (vtkIdType i = 0; i < 4 && g->GetNumberOfPoints() == 4; ++i) {
        double a[3], b[3];
        grid->GetPoint(i, a);
        g->GetPoint(i, b);
        CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);
    }
    CHECK(g->GetCellType(0) == VTK_TETRA);
    auto *areaBack = vtkDoubleArray::SafeDownCast(g->GetFieldData()->GetAbstractArray("face area"));
    CHECK(areaBack && areaBack->GetValue(0) == 0.30000000000000004 && areaBack->GetValue(1) == 1e300);
    auto *kindBack = vtkStringArray::SafeDownCast(g->GetFieldData()->GetAbstractArray("face kind"));
    CHECK(kindBack && kindBack->GetValue(1) == "B-spline surface");
    CHECK(g->GetPointData()->GetArray("node id") && g->GetCellData()->GetArray("solid"));
    // The sets and provenance come off the grid again, as they went in.
    CHECK(back.sets.size() == 3);
    bool same = back.sets.size() == mesh.sets.size();
    for (size_t i = 0; same && i < mesh.sets.size(); ++i)
        same = back.sets[i].name == mesh.sets[i].name && back.sets[i].nodes == mesh.sets[i].nodes
               && back.sets[i].members == mesh.sets[i].members;
    CHECK(same);
    CHECK(back.provenance == mesh.provenance);
    CHECK(!g->GetFieldData()->GetAbstractArray("provenance"));
    CHECK(g->GetFieldData()->GetNumberOfArrays() == 2);
}

CASE(a_file_that_is_not_there_is_said)
{
    QString error;
    const ResultsSeries back = readMeshFile("/nonexistent/mesh.vtu", &error);
    CHECK(back.points.empty());
    CHECK(error == "cannot read the mesh from /nonexistent/mesh.vtu");
}

CASE(the_worker_meshes_a_part_and_the_mesh_comes_back)
{
    QTemporaryDir dir;
    MeshRun meshing;
    const QString out = dir.filePath("mesh.vtu");
    const auto end = run(meshing, GF_STUDY, QString(GF_FIXTURES) + "/step/box.step",
                         QJsonObject{{"maxSize", 5}, {"order", 2}}, out);
    CHECK(end && end->kind == MeshEnd::Kind::Made);
    if (end && end->kind != MeshEnd::Kind::Made)
        std::printf("  (%s)\n", qPrintable(end->message));
    QString error;
    const ResultsSeries mesh = readMeshFile(out, &error);
    CHECK(error.isEmpty());
    CHECK(mesh.points.size() == 1 && mesh.points.front().grid->GetNumberOfCells() > 0);
    if (mesh.points.empty())
        return;
    CHECK(mesh.points.front().grid->GetCellType(0) == VTK_QUADRATIC_TETRA);
    int faces = 0, solids = 0;
    for (const NamedSet &s : mesh.sets) {
        faces += s.nodes && s.name.startsWith("Face") ? 1 : 0;
        solids += !s.nodes && s.name == "Solid1" ? 1 : 0;
    }
    CHECK(faces == 6 && solids == 1);
    CHECK(mesh.provenance.filter("Netgen").size() >= 1);
    CHECK(mesh.points.front().grid->GetFieldData()->GetAbstractArray("face area"));
}

CASE(the_worker_s_reason_is_passed_on)
{
    QTemporaryDir dir;
    MeshRun meshing;
    const auto end = run(meshing, GF_STUDY, QString(GF_FIXTURES) + "/step/openbox.brep",
                         QJsonObject{{"maxSize", 3}}, dir.filePath("mesh.vtu"));
    CHECK(end && end->kind == MeshEnd::Kind::Failed);
    CHECK(end && end->message.startsWith("the surface of openbox.brep does not close: 4 edges"));
    CHECK(!QFile::exists(dir.filePath("mesh.vtu")));
}

CASE(a_mesher_that_crashes_is_said_and_the_caller_goes_on)
{
    QTemporaryDir dir;
    const QString crash = standIn(dir, "crashing", "echo 'double free or corruption (out)' >&2\nkill -ABRT $$");
    MeshRun meshing;
    const auto end = run(meshing, crash, QString(GF_FIXTURES) + "/step/box.step", QJsonObject{{"maxSize", 5}},
                         dir.filePath("mesh.vtu"));
    CHECK(end && end->kind == MeshEnd::Kind::Crashed);
    CHECK(end && end->message == "the mesher stopped working while meshing box.step (it crashed; nothing of "
                                 "the study is lost). This is a fault in the mesher; its last words: double "
                                 "free or corruption (out)");
}

CASE(stop_ends_the_mesher_at_once)
{
    QTemporaryDir dir;
    const QString slow = standIn(dir, "slow", "exec sleep 60");
    MeshRun meshing;
    QElapsedTimer took;
    took.start();
    const auto end = run(meshing, slow, QString(GF_FIXTURES) + "/step/box.step", QJsonObject{{"maxSize", 5}},
                         dir.filePath("mesh.vtu"), 30, [&] { meshing.stop(); });
    CHECK(end && end->kind == MeshEnd::Kind::Stopped);
    CHECK(took.elapsed() < 5000);
    CHECK(!meshing.running());
}

CASE(a_new_meshing_replaces_the_one_running_and_only_its_end_is_said)
{
    QTemporaryDir dir;
    const QString slow = standIn(dir, "slow", "exec sleep 60");
    const QString quick = standIn(dir, "quick", "exit 0");
    MeshRun meshing;
    int ends = 0;
    QObject::connect(&meshing, &MeshRun::finished, [&](const MeshEnd &) { ++ends; });
    meshing.start(slow, "box.step", {}, dir.filePath("a.vtu"));
    const auto end = run(meshing, quick, "box.step", {}, dir.filePath("b.vtu"), 30);
    CHECK(end && end->kind == MeshEnd::Kind::Made);
    QEventLoop settle;
    QTimer::singleShot(300, &settle, &QEventLoop::quit);
    settle.exec();
    CHECK(ends == 1);
}

CASE(the_worker_is_the_installed_gf_study_else_the_one_built)
{
    QTemporaryDir installed;
    standIn(installed, "gf-study", "exit 0");
    CHECK(meshWorkerProgram(installed.path(), "/build/src/gf-study") == installed.filePath("gf-study"));
    QTemporaryDir bare;
    CHECK(meshWorkerProgram(bare.path(), "/build/src/gf-study") == "/build/src/gf-study");
}

CASE(the_end_in_words)
{
    CHECK(meshEnd(QProcess::NormalExit, 0, "", false, "a.step").kind == MeshEnd::Kind::Made);
    const MeshEnd failed = meshEnd(QProcess::NormalExit, 1, "noise\ngf-study: no solid in a.step\n", false, "a.step");
    CHECK(failed.kind == MeshEnd::Kind::Failed && failed.message == "no solid in a.step");
    const MeshEnd mute = meshEnd(QProcess::NormalExit, 1, "", false, "a.step");
    CHECK(mute.kind == MeshEnd::Kind::Failed
          && mute.message == "the mesher could not mesh a.step, and gave no reason (exit code 1)");
    CHECK(meshEnd(QProcess::CrashExit, 0, "", true, "a.step").kind == MeshEnd::Kind::Stopped);
    const MeshEnd quiet = meshEnd(QProcess::CrashExit, 0, "", false, "a.step");
    CHECK(quiet.message == "the mesher stopped working while meshing a.step (it crashed; nothing of the study "
                           "is lost). This is a fault in the mesher");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    for (auto &[name, run] : cases()) {
        const int before = failures;
        run();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
