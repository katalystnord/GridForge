// A solve stage's job folder (toolkits spec, section 4): written from a
// study's set-up and mesh, read back as the toolkit reads it, and holding
// everything it needs.

#include "core/Job.h"
#include "core/MaterialCard.h"
#include "core/Setup.h"
#include "mesh/Groups.h"
#include "mesh/Mesher.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>

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

// The box, with a material from TenSense's card and plastic block, and
// the Poisson's ratio the card lacks given by hand.
const char *setupText = R"({
  "units": "mm-N-s",
  "groups": {"Part": {"solids": [1]}, "Back": {"faces": [1]}, "Pulled": {"faces": [2]}},
  "materials": {"Measured steel": {"card": "materials/tensense-ro.FCMat",
                                   "plastic": "materials/tensense-ro-plastic.inp",
                                   "values": {"PoissonRatio": 0.3}}},
  "assign": {"Part": "Measured steel"},
  "supports": [{"group": "Back", "fixed": true}],
  "steps": [{"name": "Pull", "kind": "static",
             "loads": [{"group": "Pulled", "displacement": {"x": 0.5}}],
             "output": ["displacement", "stress"]}]
})";

struct Written {
    QTemporaryDir dir;
    QString error;
    Setup setup;
    ResultsSeries mesh;
};

Written &written()
{
    static Written w = [] {
        Written x;
        const QJsonObject json = QJsonDocument::fromJson(setupText).object();
        x.setup = readSetup(json, &x.error);
        if (x.error.isEmpty())
            fillFromCards(x.setup, GF_FIXTURES, &x.error);
        x.mesh = gf::mesh::meshStep(QString(GF_FIXTURES) + "/step/box.step", {5, 2}, &x.error);
        if (x.error.isEmpty())
            gf::mesh::addGroupSets(x.mesh, x.setup, &x.error);
        if (x.error.isEmpty())
            writeSolveJob(x.dir.path(), "structural.static", x.setup, json, GF_FIXTURES, x.mesh, &x.error);
        return x;
    }();
    return w;
}

} // namespace

CASE(the_job_folder_holds_what_the_spec_lists)
{
    Written &w = written();
    CHECK(w.error.isEmpty());
    if (!w.error.isEmpty())
        std::printf("  (%s)\n", qPrintable(w.error));
    const QDir d(w.dir.path());
    CHECK(d.exists("job.json") && d.exists("mesh/mesh.vtu"));
    CHECK(d.exists("results") && d.exists("work"));
    CHECK(d.exists("materials/Measured_steel-plastic.inp"));
    QFile copied(d.filePath("materials/Measured_steel-plastic.inp")),
        original(QString(GF_FIXTURES) + "/materials/tensense-ro-plastic.inp");
    CHECK(copied.open(QIODevice::ReadOnly) && original.open(QIODevice::ReadOnly)
          && copied.readAll() == original.readAll());
}

CASE(job_json_holds_the_set_up_resolved)
{
    Written &w = written();
    QString error;
    const SolveJob job = readSolveJob(w.dir.path(), &error);
    CHECK(error.isEmpty() && job.kind == "structural.static");
    const QJsonObject m = job.setup["materials"].toObject()["Measured steel"].toObject();
    CHECK(!m.contains("card"));  // its values are in the job, not a file outside it
    CHECK(m["plastic"].toString() == "materials/Measured_steel-plastic.inp");
    CHECK(m["values"].toObject()["YoungsModulus"].toDouble() == 199900);
    CHECK(m["values"].toObject()["PoissonRatio"].toDouble() == 0.3);
    CHECK(m["source"].toObject()["YoungsModulus"].toString() == "199.9 GPa in tensense-ro.FCMat");
    CHECK(m["source"].toObject()["PoissonRatio"].toString() == "entered in the study");
    // The toolkit reads it as a set-up, with each value's source.
    const Setup s = readSetup(job.setup, &error);
    CHECK(error.isEmpty() && s.material("Measured steel")
          && s.material("Measured steel")->source.at("YoungsModulus") == "199.9 GPa in tensense-ro.FCMat");
}

CASE(the_mesh_comes_back_with_its_ids_and_sets)
{
    Written &w = written();
    QString error;
    const SolveJob job = readSolveJob(w.dir.path(), &error);
    CHECK(!job.mesh.points.empty());
    if (job.mesh.points.empty())
        return;
    vtkUnstructuredGrid *a = w.mesh.points.front().grid, *b = job.mesh.points.front().grid;
    CHECK(a->GetNumberOfPoints() == b->GetNumberOfPoints() && a->GetNumberOfCells() == b->GetNumberOfCells());
    bool same = true;
    for (vtkIdType p = 0; same && p < a->GetNumberOfPoints(); ++p) {
        double x[3], y[3];
        a->GetPoint(p, x);
        b->GetPoint(p, y);
        same = x[0] == y[0] && x[1] == y[1] && x[2] == y[2];  // bit for bit
    }
    CHECK(same);
    CHECK(b->GetPointData()->GetArray("node id") && b->GetCellData()->GetArray("element id"));
    // The sets come back as sets, not as arrays left on the grid.
    bool noFlags = true;
    for (int k = 0; k < b->GetPointData()->GetNumberOfArrays(); ++k)
        noFlags = noFlags && !QString(b->GetPointData()->GetArrayName(k)).startsWith("set ");
    for (int k = 0; k < b->GetCellData()->GetNumberOfArrays(); ++k)
        noFlags = noFlags && !QString(b->GetCellData()->GetArrayName(k)).startsWith("set ");
    CHECK(noFlags);
    CHECK(job.mesh.sets.size() == w.mesh.sets.size());
    for (const NamedSet &set : w.mesh.sets) {
        bool found = false;
        for (const NamedSet &read : job.mesh.sets)
            found = found || (read.name == set.name && read.nodes == set.nodes && read.members == set.members);
        CHECK(found);
        if (!found)
            std::printf("  (set %s, %s)\n", qPrintable(set.name), set.nodes ? "nodes" : "elements");
    }
}

CASE(a_folder_that_is_not_a_job_is_said)
{
    QTemporaryDir empty;
    QString error;
    const SolveJob job = readSolveJob(empty.path(), &error);
    CHECK(job.kind.isEmpty());
    CHECK(error == "no job.json in " + empty.path());
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
