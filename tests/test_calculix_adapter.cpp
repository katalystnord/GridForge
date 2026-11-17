// gf-calculix, the CalculiX toolkit's adapter (toolkits spec, sections 4
// and 5): given a job folder it writes the deck into work/, runs ccx
// there, says its progress as JSON lines, and leaves the results in
// results/. Run here with a stand-in ccx (fixtures/fake-ccx) that replays
// what ccx 2.23 wrote for box-setup, so the test needs no ccx; the job is
// box-setup's, so the replayed results fit its mesh.

#include "core/Job.h"
#include "core/MaterialCard.h"
#include "core/Setup.h"
#include "mesh/Groups.h"
#include "mesh/Mesher.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include <vtkDataAssembly.h>
#include <vtkHDFReader.h>
#include <vtkNew.h>
#include <vtkPartitionedDataSetCollection.h>

#include <csignal>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::core;

#if !defined(GF_FIXTURES) || !defined(GF_ADAPTER) || !defined(GF_STUDY)
#error "GF_FIXTURES, GF_ADAPTER and GF_STUDY must be defined"
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

const QString fixtures = GF_FIXTURES;

// A fixture's job (box-setup's unless named), written into `folder`;
// `change` may alter its set-up first.
bool makeJob(const QString &folder, const QString &fixture = "box-setup",
             const std::function<void(QJsonObject &)> &change = {})
{
    QFile f(fixtures + "/calculix/" + fixture + "/tension.json");
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonObject json = QJsonDocument::fromJson(f.readAll()).object()["setup"].toObject();
    if (change)
        change(json);
    QString error;
    Setup setup = readSetup(json, &error);
    fillFromCards(setup, fixtures, &error);
    ResultsSeries mesh = gf::mesh::meshStep(fixtures + "/step/box.step", {5, 2}, &error);
    gf::mesh::addGroupSets(mesh, setup, &error);
    return writeSolveJob(folder, "structural.static", setup, json, fixtures, mesh, &error);
}

struct Run {
    int exitCode = -1;
    std::vector<QJsonObject> lines;  // standard output, one JSON object each
    QStringList notJson;
};

Run run(const QString &job, const QString &ccx, const QString &fail = {}, const QString &from = "box-setup")
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GF_CCX", ccx);
    env.insert("FAKE_CCX_FROM", fixtures + "/calculix/" + from);
    if (!fail.isEmpty())
        env.insert("FAKE_CCX_FAIL", fail);
    p.setProcessEnvironment(env);
    p.start(GF_ADAPTER, {job});
    p.waitForFinished(60000);
    Run r;
    r.exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    for (const QByteArray &line : p.readAllStandardOutput().split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        const QJsonDocument d = QJsonDocument::fromJson(line);
        if (d.isObject() && d.object().contains("type"))
            r.lines.push_back(d.object());
        else
            r.notJson << QString::fromUtf8(line);
    }
    return r;
}

const QJsonObject *first(const Run &r, const QString &type, const QString &level = {})
{
    for (const QJsonObject &o : r.lines)
        if (o["type"].toString() == type && (level.isEmpty() || o["level"].toString() == level))
            return &o;
    return nullptr;
}

const QString fake = fixtures + "/fake-ccx/ccx";

} // namespace

CASE(a_job_is_solved_and_its_results_left_in_results)
{
    QTemporaryDir dir;
    CHECK(makeJob(dir.path()));
    const Run r = run(dir.path(), fake);
    CHECK(r.exitCode == 0);
    CHECK(r.notJson.isEmpty());  // standard output is the protocol only
    for (const QString &l : r.notJson)
        std::printf("  (not JSON: %s)\n", qPrintable(l));
    const QDir d(dir.path());
    CHECK(d.exists("work/job.inp") && d.exists("work/mesh.inp"));
    CHECK(d.exists("work/job.frd") && d.exists("work/ccx.log"));  // ccx's own files kept
    CHECK(d.exists("results/results.vtkhdf") && d.exists("results/results.json"));
    const QJsonObject *file = first(r, "file");
    CHECK(file && (*file)["path"].toString() == "results/results.vtkhdf");
}

CASE(progress_is_said_as_the_increments_finish)
{
    QTemporaryDir dir;
    makeJob(dir.path());
    const Run r = run(dir.path(), fake);
    bool done = false;
    for (const QJsonObject &o : r.lines)
        if (o["type"].toString() == "progress" && o["fraction"].toDouble() == 1.0
            && o["text"].toString() == "step 1, increment 1")
            done = true;
    CHECK(done);
}

CASE(the_results_say_who_solved_them_and_hold_the_sets)
{
    QTemporaryDir dir;
    makeJob(dir.path());
    run(dir.path(), fake);
    QFile f(QDir(dir.path()).filePath("results/results.json"));
    CHECK(f.open(QIODevice::ReadOnly));
    const QJsonObject j = QJsonDocument::fromJson(f.readAll()).object();
    CHECK(j["format"].toString() == "gridforge-results");
    CHECK(j["toolkit"].toString() == "calculix");
    CHECK(j["solver"].toString() == "CalculiX Version 2.23");
    CHECK(j["trust"].toString() == "solved");
    CHECK(j["files"].toArray() == QJsonArray({"results/results.vtkhdf"}));
    CHECK(j["provenance"].toArray().size() > 2);

    vtkNew<vtkHDFReader> reader;
    reader->SetFileName(QDir(dir.path()).filePath("results/results.vtkhdf").toUtf8().constData());
    reader->Update();
    auto *pdc = vtkPartitionedDataSetCollection::SafeDownCast(reader->GetOutputDataObject(0));
    CHECK(pdc && pdc->GetDataAssembly());
    QStringList names;
    if (pdc && pdc->GetDataAssembly()) {
        vtkDataAssembly *a = pdc->GetDataAssembly();
        std::function<void(int)> walk = [&](int node) {
            names << a->GetNodeName(node);
            for (int child : a->GetChildNodes(node, false))
                walk(child);
        };
        walk(0);
    }
    CHECK(names.contains("Pulled") && names.contains("Steel_part") && names.contains("Model"));
}

CASE(a_failing_ccx_is_said_with_its_own_words)
{
    QTemporaryDir dir;
    makeJob(dir.path());
    const Run r = run(dir.path(), fake, "1");
    CHECK(r.exitCode == 1);
    const QJsonObject *error = first(r, "log", "error");
    CHECK(error && (*error)["text"].toString().contains("*ERROR reading *MATERIAL"));
    CHECK(!QDir(dir.path()).exists("results/results.vtkhdf"));
}

CASE(an_error_ccx_prints_is_a_failure_whatever_its_exit_code)
{
    // Defensive: no ccx has been seen to print *ERROR and exit 0; if one
    // does, its results are not passed on.
    QTemporaryDir dir;
    makeJob(dir.path());
    const Run r = run(dir.path(), fake, "quiet");
    CHECK(r.exitCode == 1);
    CHECK(!QDir(dir.path()).exists("results/results.vtkhdf"));
}

CASE(progress_follows_the_steps)
{
    // box-twosteps, solved by ccx 2.23: total time 1 after step 1, 2 after
    // step 2, so half way, then done.
    QTemporaryDir dir;
    CHECK(makeJob(dir.path(), "box-twosteps"));
    const Run r = run(dir.path(), fake, {}, "box-twosteps");
    QStringList progress;
    for (const QJsonObject &o : r.lines)
        if (o["type"].toString() == "progress")
            progress << QString("%1 %2").arg(o["fraction"].toDouble()).arg(o["text"].toString());
    CHECK(progress == QStringList({"0.5 step 1, increment 1", "1 step 2, increment 1"}));
}

CASE(a_plastic_block_is_included_from_where_ccx_runs)
{
    QTemporaryDir dir;
    CHECK(makeJob(dir.path(), "box-setup", [](QJsonObject &o) {
        QJsonObject materials = o["materials"].toObject(), steel = materials["Steel"].toObject();
        steel["plastic"] = "materials/tensense-ro-plastic.inp";
        materials["Steel"] = steel;
        o["materials"] = materials;
    }));
    run(dir.path(), fake);
    QFile deck(QDir(dir.path()).filePath("work/job.inp"));
    CHECK(deck.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(deck.readAll());
    CHECK(text.contains("*INCLUDE, INPUT=../materials/Steel-plastic.inp\n"));
    CHECK(QFileInfo(QDir(dir.path()).filePath("work/../materials/Steel-plastic.inp")).isFile());
    CHECK(text.contains("*STEP, NLGEOM"));
}

// Starts `program args` with the stand-in ccx made slow; returns once a
// line holding `ready` has been printed (or 20 s have passed).
struct Slow {
    QProcess p;
    QByteArray out;
    bool start(const QString &program, const QStringList &args, const QByteArray &ready)
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("GF_CCX", fake);
        env.insert("FAKE_CCX_FROM", fixtures + "/calculix/box-setup");
        env.insert("FAKE_CCX_SLOW", "1");
        p.setProcessEnvironment(env);
        p.start(program, args);
        for (int k = 0; k < 200 && !out.contains(ready); ++k) {
            p.waitForReadyRead(100);
            out += p.readAllStandardOutput();
        }
        return out.contains(ready);
    }
    void finish(int ms)
    {
        p.waitForFinished(ms);
        out += p.readAllStandardOutput();
    }
};

CASE(a_solve_is_cancelled_by_sigterm_and_says_so)
{
    // The application stops a toolkit with SIGTERM (toolkits spec, 5):
    // the adapter stops ccx, keeps its files, says where it had got to,
    // leaves no results and exits with 3.
    QTemporaryDir dir;
    makeJob(dir.path());
    Slow s;
    CHECK(s.start(GF_ADAPTER, {dir.path()}, "step 1, increment 1"));
    QElapsedTimer clock;
    clock.start();
    s.p.terminate();
    s.finish(15000);
    CHECK(s.p.state() == QProcess::NotRunning && clock.elapsed() < 10000);
    CHECK(s.p.exitStatus() == QProcess::NormalExit && s.p.exitCode() == 3);
    CHECK(s.out.contains(R"("text":"cancelled: ccx stopped after step 1, increment 1")"));
    const QDir d(dir.path());
    CHECK(!d.exists("results/results.vtkhdf"));
    CHECK(d.exists("work/job.inp") && d.exists("work/job.sta"));  // kept
}

CASE(ctrl_c_cancels_a_solve_from_the_command_line)
{
    QTemporaryDir dir;
    Slow s;
    CHECK(s.start(GF_STUDY,
                  {"solve", fixtures + "/calculix/box-setup/tension.json", fixtures + "/step/box.step", "--size",
                   "5", "--order", "2", "-o", dir.path()},
                  "step 1, increment 1"));
    QElapsedTimer clock;
    clock.start();
    ::kill(static_cast<pid_t>(s.p.processId()), SIGINT);
    s.finish(15000);
    CHECK(s.p.state() == QProcess::NotRunning);
    CHECK(s.p.exitCode() == 3);
    // The toolkit asked, not killed: it said where ccx had got to, at once.
    CHECK(s.out.contains("cancelled: ccx stopped after step 1, increment 1"));
    CHECK(clock.elapsed() < 9000);
    CHECK(!QDir(dir.path()).exists("results/results.vtkhdf"));
}

CASE(no_ccx_is_said_before_anything_runs)
{
    QTemporaryDir dir;
    makeJob(dir.path());
    const Run r = run(dir.path(), "/no/such/ccx");
    CHECK(r.exitCode == 2);
    const QJsonObject *error = first(r, "log", "error");
    CHECK(error && (*error)["text"].toString() == "ccx not found: /no/such/ccx");
}

CASE(a_folder_that_is_not_a_job_is_refused)
{
    QTemporaryDir dir;
    const Run r = run(dir.path(), fake);
    CHECK(r.exitCode == 2);
    const QJsonObject *error = first(r, "log", "error");
    CHECK(error && (*error)["text"].toString().startsWith("no job.json in "));
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
