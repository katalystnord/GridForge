// gf-calculix: the CalculiX toolkit's adapter (docs/specs/toolkits-v1.md).
//
//   gf-calculix JOB
//
// Reads the job folder (job.json, mesh/mesh.vtu, materials/), writes the
// deck into JOB/work (mesh.inp and job.inp), runs ccx there as its own
// process, and says what happens as one JSON object per line on standard
// output (section 5): progress per increment from job.sta, the force
// residual per iteration from job.cvg, errors in ccx's own words, and the
// results file. The results go to JOB/results: results.vtkhdf (with the
// mesh's sets) and results.json (who solved them, how far to trust them).
// ccx's own files stay in work/ (tenet 10).
//
// ccx is the program GF_CCX names, else "ccx" on PATH. Exit codes: 0
// solved; 1 ccx failed; 2 the job could not be started; 3 cancelled.
//
// Cancelling (the application sends SIGTERM): ccx is stopped (SIGTERM,
// then SIGKILL after 5 s), its files stay in work/, no results are
// written, and the last line says where it had got to.
//
// ccx is GPL-2.0-only and is run, never linked.

#include "calculix/DeckWriter.h"
#include "calculix/FrdToVtk.h"
#include "calculix/InpSets.h"
#include "calculix/InpWriter.h"
#include "calculix/JobProgress.h"
#include "core/ApplicationInfo.h"
#include "core/Job.h"
#include "core/Setup.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <csignal>
#include <cstdio>

namespace {

volatile std::sig_atomic_t cancelled = 0;
extern "C" void onTerm(int) { cancelled = 1; }

void say(const QJsonObject &message)
{
    std::fputs(QJsonDocument(message).toJson(QJsonDocument::Compact).constData(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

int refuse(const QString &why, int code)
{
    say({{"type", "log"}, {"level", code == 3 ? "info" : "error"}, {"text", why}});
    return code;
}

bool writeText(const QString &path, const QString &text)
{
    QSaveFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(text.toUtf8()) >= 0 && f.commit();
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    struct sigaction term = {};
    term.sa_handler = onTerm;
    sigaction(SIGTERM, &term, nullptr);
    if (argc != 2)
        return refuse("usage: gf-calculix JOB", 2);
    const QDir job(QString::fromLocal8Bit(argv[1]));

    // The job, its deck.
    QString error;
    const gf::core::SolveJob solve = gf::core::readSolveJob(job.path(), &error);
    if (!error.isEmpty())
        return refuse(error, 2);
    gf::core::Setup setup = gf::core::readSetup(solve.setup, &error);
    if (!error.isEmpty())
        return refuse("job.json: " + error, 2);
    for (gf::core::Material &m : setup.materials)
        if (!m.plastic.isEmpty())
            m.plastic = "../" + m.plastic;  // ccx runs in work/
    const QString deck = gf::calculix::analysisInp(setup, solve.mesh, "mesh.inp", &error);
    if (deck.isEmpty())
        return refuse(error, 2);
    const QDir work(job.filePath("work"));
    if (!gf::calculix::writeMeshInp(solve.mesh, work.filePath("mesh.inp"), &error))
        return refuse(error, 2);
    if (!writeText(work.filePath("job.inp"), deck))
        return refuse("cannot write " + work.filePath("job.inp"), 2);
    // Said at once: the deck matters most when the solve then fails.
    say({{"type", "file"}, {"path", "work/job.inp"}, {"role", "deck"}});

    // ccx.
    const QString named = qEnvironmentVariable("GF_CCX", "ccx");
    const QString ccx = QFileInfo(named).isAbsolute() || named.contains('/')
                            ? (QFileInfo(named).isExecutable() ? QFileInfo(named).absoluteFilePath() : QString())
                            : QStandardPaths::findExecutable(named);
    if (ccx.isEmpty())
        return refuse("ccx not found: " + named, 2);
    for (const char *old : {"job.sta", "job.cvg", "job.frd", "job.dat"})
        QFile::remove(work.filePath(old));
    QProcess process;
    process.setWorkingDirectory(work.path());
    process.setStandardOutputFile(work.filePath("ccx.log"));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(ccx, {"-i", "job"});
    if (!process.waitForStarted(10000))
        return refuse("ccx did not start: " + ccx, 2);
    say({{"type", "log"}, {"level", "info"}, {"text", "ccx started: " + ccx}});

    // Progress, from what ccx appends as it goes.
    const int steps = std::max<int>(1, static_cast<int>(setup.steps.size()));
    size_t staSeen = 0, cvgSeen = 0;
    QString reached;  // the last increment done, for a cancel
    auto follow = [&] {
        const gf::calculix::StaFile sta = gf::calculix::parseSta(readAll(work.filePath("job.sta")));
        for (; staSeen < sta.rows.size(); ++staSeen) {
            const auto &row = sta.rows[staSeen];
            if (!row.converged)
                continue;
            reached = QString("step %1, increment %2").arg(row.step).arg(row.increment);
            say({{"type", "progress"}, {"fraction", std::min(1.0, row.totalTime / steps)}, {"text", reached}});
        }
        const gf::calculix::CvgFile cvg = gf::calculix::parseCvg(readAll(work.filePath("job.cvg")));
        for (; cvgSeen < cvg.rows.size(); ++cvgSeen)
            say({{"type", "metric"},
                 {"name", "force residual"},
                 {"value", cvg.rows[cvgSeen].forceResidual},
                 {"text", QString("step %1, increment %2, iteration %3")
                              .arg(cvg.rows[cvgSeen].step)
                              .arg(cvg.rows[cvgSeen].increment)
                              .arg(cvg.rows[cvgSeen].iteration)}});
    };
    while (!process.waitForFinished(200)) {
        follow();
        if (cancelled) {
            process.terminate();
            if (!process.waitForFinished(5000)) {
                process.kill();
                process.waitForFinished(5000);
            }
            follow();
            return refuse(reached.isEmpty() ? QString("cancelled: before ccx finished its first increment")
                                            : "cancelled: ccx stopped after " + reached,
                          3);
        }
    }
    follow();

    const QString log = QString::fromUtf8(readAll(work.filePath("ccx.log")));
    const QRegularExpressionMatch version = QRegularExpression("CalculiX Version [0-9.]+").match(log);
    const qsizetype at = log.indexOf("*ERROR");
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || at >= 0) {
        QString last;  // with no *ERROR, the log's last words say most
        for (const QString &line : log.split('\n', Qt::SkipEmptyParts))
            last = line.simplified();
        const QString words = at >= 0 ? log.mid(at).section('\n', 0, 1).simplified()
                                      : QString("ccx stopped with exit code %1%2")
                                            .arg(process.exitCode())
                                            .arg(last.isEmpty() ? QString() : ": " + last);
        return refuse(words, 1);
    }

    // The results, with the mesh's sets.
    const gf::calculix::FrdFile frd = gf::calculix::readFrd(work.filePath("job.frd"));
    if (!frd.ok())
        return refuse("job.frd: " + frd.error, 1);
    gf::core::ResultsSeries results = gf::calculix::toSeries(frd, "job.frd", &error);
    if (!error.isEmpty())
        return refuse("job.frd: " + error, 1);
    const gf::calculix::InpSets sets = gf::calculix::readInpSets(work.filePath("mesh.inp"));
    if (sets.ok())
        gf::calculix::addSets(results, sets, "mesh.inp");
    const QStringList notes = gf::calculix::extrapolationNotes(results);
    for (const QString &note : notes)
        results.provenance << "note: " + note;
    results.provenance << QString("toolkit: gf-calculix (%1 %2, build %3)")
                              .arg(gf::core::applicationName(), gf::core::applicationVersion(),
                                   gf::core::applicationBuild());
    const QString out = job.filePath("results/results.vtkhdf");
    if (!gf::core::writeVtkHdf(results, out, &error))
        return refuse(error, 1);
    QJsonArray provenance;
    for (const QString &line : results.provenance)
        provenance.append(line);
    QJsonArray times;  // also in the .vtkhdf; listed here for readers that drop a single one
    for (const gf::core::TimePoint &t : results.points)
        times.append(t.time);
    const QJsonObject report{{"format", "gridforge-results"},
                             {"version", "1.0"},
                             {"toolkit", "calculix"},
                             {"toolkit_version", gf::core::applicationVersion()},
                             {"solver", version.hasMatch() ? version.captured(0) : QString("CalculiX (version not said)")},
                             {"trust", "solved"},
                             {"files", QJsonArray{"results/results.vtkhdf"}},
                             {"deck", "work/job.inp"},
                             {"time points", times},
                             {"notes", QJsonArray::fromStringList(notes)},
                             {"provenance", provenance}};
    if (!writeText(job.filePath("results/results.json"), QJsonDocument(report).toJson()))
        return refuse("cannot write " + job.filePath("results/results.json"), 1);
    for (const QString &note : notes)
        say({{"type", "log"}, {"level", "warning"}, {"text", note}});
    say({{"type", "file"}, {"path", "results/results.vtkhdf"}});
    return 0;
}
