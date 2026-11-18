// gf-study: the application's work without a window, for scripts, CI and
// the test suite.
//
//   gf-study --version
//   gf-study convert job.frd [-o DIR] [--deck job.inp] [--pvd]
//       results to one .vtkhdf for ParaView, with the deck's named sets as
//       blocks (the deck: job.inp beside the results, unless named); with
//       --pvd, a .vtu series and its .pvd instead (no sets)
//   gf-study convert mesh.msh [-o DIR] [--pvd]
//       a Gmsh mesh likewise, its physical groups as the sets
//   gf-study mesh part.step --size H [--order 2] [-o DIR] [--pvd] [--inp]
//       the STEP's solids meshed with tetrahedra (gf_mesh, Netgen), to one
//       .vtkhdf with each solid and face as a set; with --inp, also
//       <name>.inp, the mesh and its sets for a CalculiX deck to *INCLUDE
//   gf-study deck setup.json part.step --size H [--order 2] [-o DIR]
//       a CalculiX job from a set-up (study spec, section 4; the file holds
//       the "setup" object): the part meshed, its groups as sets, written
//       to mesh.inp, and the analysis deck <setup name>.inp around it
//   gf-study solve setup.json part.step --size H [--order 2] -o JOB
//       the same set-up solved: the job folder written (toolkits spec,
//       section 4), the solve toolkit found by its manifest and run, its
//       progress shown; the results left in JOB/results
//   gf-study converge setup.json part.step --size H [--ratio 1.5] -o DIR
//       the set-up solved on three meshes, each finer than the last by the
//       ratio in every size, in DIR/mesh-1..3; how far the largest
//       displacement and von Mises stress can be trusted against the mesh
//       (core/Convergence), on the terminal and in DIR/convergence.json
//   gf-study mesh-worker part.step --recipe JSON -o mesh.vtu
//       what the window runs to mesh (core/MeshRun): the part meshed by
//       the mesh step's parameters as study.json keeps them, to one .vtu
//       holding the mesh exactly, with its sets and provenance; a reason
//       on standard error, and exit code 1, if it cannot
//   Both mesh commands measure the mesh: the verdict and measures on the
//   terminal, the report for agents in <name>.quality.json beside it, and
//   each cell's "mean ratio" on the grid for ParaView.

#include "calculix/FrdToVtk.h"
#include "calculix/DeckWriter.h"
#include "calculix/InpWriter.h"
#include "core/Convergence.h"
#include "core/Job.h"
#include "core/MeshRun.h"
#include "core/JobRunner.h"
#include "core/MaterialCard.h"
#include "core/Toolkit.h"
#include "core/Setup.h"
#include "mesh/Groups.h"
#include "core/ApplicationInfo.h"
#include "gmsh/MshToVtk.h"
#include "mesh/Mesher.h"
#include "mesh/Quality.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QSaveFile>

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>

#include <csignal>
#include <cstdio>

namespace {

// Writes the series beside the input (or into outDir); the path written,
// or empty with *error set.
QString write(const gf::core::ResultsSeries &series, const QFileInfo &in, const QString &outDir,
              bool pvd, QString *error)
{
    const QString dir = outDir.isEmpty() ? in.absolutePath() : outDir;
    if (!QDir().mkpath(dir)) {
        *error = QString("cannot make the folder %1").arg(dir);
        return {};
    }
    if (pvd)
        return gf::core::writeVtuSeries(series, dir, in.completeBaseName(), error).value(0);
    const QString path = QDir(dir).filePath(in.completeBaseName() + ".vtkhdf");
    return gf::core::writeVtkHdf(series, path, error) ? path : QString();
}

// Measures the mesh, puts each cell's mean ratio on the grid, and says
// where the report for agents will go. Call before writing the mesh.
gf::mesh::Quality measureMesh(const gf::core::ResultsSeries &series)
{
    vtkUnstructuredGrid *grid = series.points.front().grid;
    const gf::mesh::Quality quality = gf::mesh::measure(grid);
    vtkNew<vtkDoubleArray> ratio;
    ratio->SetName("mean ratio");  // -1 where not measured
    for (double r : quality.meanRatio)
        ratio->InsertNextValue(r);
    grid->GetCellData()->AddArray(ratio);
    return quality;
}

// Writes <mesh>.quality.json beside the mesh written and prints the
// summary; false with a message if the report cannot be written.
bool reportQuality(const gf::mesh::Quality &quality, vtkUnstructuredGrid *grid, const QString &written)
{
    const QFileInfo out(written);
    const QString path = out.absoluteDir().filePath(out.completeBaseName() + ".quality.json");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(gf::mesh::toJson(quality, grid)).toJson()) < 0 || !file.commit()) {
        std::fprintf(stderr, "gf-study: cannot write %s\n", qPrintable(path));
        return false;
    }
    std::printf("quality (%s):\n", qPrintable(path));
    for (const QString &line : gf::mesh::summary(quality))
        std::printf("  %s\n", qPrintable(line));
    return true;
}

int convertMesh(const QString &input, const QString &outDir, bool pvd)
{
    const QFileInfo in(input);
    const gf::gmsh::MshFile msh = gf::gmsh::readMsh(input);
    QString error;
    const gf::core::ResultsSeries series = gf::gmsh::toSeries(msh, in.fileName(), &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s: %s\n", qPrintable(input), qPrintable(error));
        return 1;
    }
    const gf::mesh::Quality quality = measureMesh(series);
    const QString written = write(series, in, outDir, pvd, &error);
    if (written.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    const vtkIdType cells = series.points.front().grid->GetNumberOfCells();
    const QString sets = pvd ? QString("no sets: a .vtu series has no blocks")
                             : QString("%1 set%2 from its physical groups")
                                   .arg(series.sets.size())
                                   .arg(series.sets.size() == 1 ? "" : "s");
    std::printf("wrote %s (a mesh: %zu nodes, %lld elements, %s)\n", qPrintable(written),
                msh.nodes.size(), static_cast<long long>(cells), qPrintable(sets));
    for (const QString &line : series.provenance)
        std::printf("  %s\n", qPrintable(line));
    return reportQuality(quality, series.points.front().grid, written) ? 0 : 1;
}

// --through-wall onto a recipe: elements through a thin wall, a whole
// number, 0 or more; the recipe's default when not given.
bool throughWallOnto(gf::mesh::Recipe &recipe, const QString &throughWall)
{
    if (throughWall.isEmpty())
        return true;
    bool ok = false;
    const int n = throughWall.toInt(&ok);
    if (!ok || n < 0) {
        std::fprintf(stderr, "gf-study: --through-wall is a whole number, 0 or more (0: none added), not %s\n",
                     qPrintable(throughWall));
        return false;
    }
    recipe.throughWall = n;
    return true;
}

int mesh(const QString &input, const QString &size, const QString &order, const QString &throughWall,
         const QString &outDir, bool pvd, bool inp)
{
    bool sizeOk = false, orderOk = false;
    gf::mesh::Recipe recipe{size.toDouble(&sizeOk), order.isEmpty() ? 1 : order.toInt(&orderOk)};
    if (!throughWallOnto(recipe, throughWall))
        return 1;
    if (!sizeOk) {
        std::fprintf(stderr, "gf-study: mesh needs --size, the largest element edge\n");
        return 1;
    }
    if (!order.isEmpty() && !orderOk) {
        std::fprintf(stderr, "gf-study: --order is 1 or 2, not %s\n", qPrintable(order));
        return 1;
    }
    const QFileInfo in(input);
    QString error;
    const gf::core::ResultsSeries series = gf::mesh::meshStep(input, recipe, &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    const gf::mesh::Quality quality = measureMesh(series);
    const QString written = write(series, in, outDir, pvd, &error);
    if (written.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    vtkUnstructuredGrid *grid = series.points.front().grid;
    const QString sets = pvd ? QString("no sets: a .vtu series has no blocks")
                             : QString("%1 sets from its solids and faces").arg(series.sets.size());
    std::printf("wrote %s (a mesh: %lld nodes, %lld elements, %s)\n", qPrintable(written),
                static_cast<long long>(grid->GetNumberOfPoints()),
                static_cast<long long>(grid->GetNumberOfCells()), qPrintable(sets));
    for (const QString &line : series.provenance)
        std::printf("  %s\n", qPrintable(line));
    if (inp) {
        const QString deck = QFileInfo(written).absoluteDir().filePath(in.completeBaseName() + ".inp");
        if (!gf::calculix::writeMeshInp(series, deck, &error)) {
            std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
            return 1;
        }
        std::printf("wrote %s (the mesh and its sets, for a CalculiX deck to *INCLUDE)\n", qPrintable(deck));
    }
    return reportQuality(quality, grid, written) ? 0 : 1;
}

// The window's meshing, in a process of its own (core/MeshRun): a crash in
// the engine ends this process, not the window.
int meshWorker(const QString &input, const QString &recipeJson, const QString &out)
{
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(recipeJson.toUtf8(), &parse);
    if (out.isEmpty() || parse.error != QJsonParseError::NoError || !doc.isObject()) {
        std::fprintf(stderr, "gf-study: mesh-worker needs --recipe, the mesh step's parameters as JSON, "
                             "and -o, the file to write\n");
        return 1;
    }
    QString error;
    const gf::core::ResultsSeries series = gf::mesh::meshStep(input, gf::mesh::recipeFrom(doc.object()), &error);
    if (error.isEmpty())
        gf::core::writeMeshFile(series, out, &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    return 0;
}

// The set-up read and filled from its cards, and the part meshed with its
// groups as sets: what both deck and solve start from.
bool prepare(const QString &setupFile, const QString &part, const QString &size, const QString &order,
             const QString &throughWall,
             gf::core::Setup *setup, QJsonObject *setupJson, gf::core::ResultsSeries *mesh, double finer = 1)
{
    QFile f(setupFile);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "gf-study: cannot read %s\n", qPrintable(setupFile));
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError) {
        std::fprintf(stderr, "gf-study: %s: %s at byte %d\n", qPrintable(setupFile),
                     qPrintable(parse.errorString()), parse.offset);
        return false;
    }
    QString error;
    *setupJson = doc.object()["setup"].toObject();
    *setup = gf::core::readSetup(*setupJson, &error);
    if (error.isEmpty())
        gf::core::fillFromCards(*setup, QFileInfo(setupFile).absolutePath(), &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s: %s\n", qPrintable(setupFile), qPrintable(error));
        return false;
    }
    bool sizeOk = false, orderOk = false;
    gf::mesh::Recipe recipe{size.toDouble(&sizeOk), order.isEmpty() ? 1 : order.toInt(&orderOk)};
    if (!throughWallOnto(recipe, throughWall))
        return false;
    if (!sizeOk || (!order.isEmpty() && !orderOk)) {
        std::fprintf(stderr, "gf-study: --size is needed, and --order is 1 or 2\n");
        return false;
    }
    recipe.finer = finer;
    *mesh = gf::mesh::meshStep(part, recipe, &error);
    if (error.isEmpty())
        gf::mesh::addGroupSets(*mesh, *setup, &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return false;
    }
    return true;
}

int solvePrepared(const QString &setupFile, const gf::core::Setup &setup, const QJsonObject &setupJson,
                  const gf::core::ResultsSeries &mesh, const QString &jobDir, QString *resultsOut);

volatile std::sig_atomic_t interrupted = 0;
extern "C" void onInterrupt(int) { interrupted = 1; }

int solve(const QString &setupFile, const QString &part, const QString &size, const QString &order,
          const QString &throughWall, const QString &jobDir)
{
    if (jobDir.isEmpty()) {
        std::fprintf(stderr, "gf-study: solve needs -o JOB, the job folder to write\n");
        return 1;
    }
    gf::core::Setup setup;
    QJsonObject setupJson;
    gf::core::ResultsSeries mesh;
    if (!prepare(setupFile, part, size, order, throughWall, &setup, &setupJson, &mesh))
        return 1;
    return solvePrepared(setupFile, setup, setupJson, mesh, jobDir, nullptr);
}

// A prepared set-up and mesh solved in jobDir by its toolkit, as the
// window does; 0 when solved (the results file in *results), 3 when
// cancelled, 1 otherwise.
int solvePrepared(const QString &setupFile, const gf::core::Setup &setup, const QJsonObject &setupJson,
                  const gf::core::ResultsSeries &mesh, const QString &jobDir, QString *resultsOut)
{
    QString error;
    if (!gf::core::writeSolveJob(jobDir, "structural.static", setup, setupJson,
                                 QFileInfo(setupFile).absolutePath(), mesh, &error)) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    std::printf("wrote the job %s (%lld nodes, %lld elements)\n", qPrintable(QDir(jobDir).absolutePath()),
                static_cast<long long>(mesh.points.front().grid->GetNumberOfPoints()),
                static_cast<long long>(mesh.points.front().grid->GetNumberOfCells()));

    // The solve toolkit, by its manifest; this program's own folder is
    // searched for its command first (the shipped adapters sit beside it).
    qputenv("PATH", (QCoreApplication::applicationDirPath() + ":" + qEnvironmentVariable("PATH")).toLocal8Bit());
    const gf::core::ToolkitSet toolkits = gf::core::findToolkits(gf::core::shippedToolkitFolders(QCoreApplication::applicationDirPath(), GF_SHIPPED_TOOLKITS));
    QString why;
    const gf::core::Toolkit *toolkit = gf::core::readyToolkit(toolkits, "solve", "structural.static", &why);
    if (!toolkit) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(why));
        return 1;
    }
    std::printf("solving with %s (%s)\n", qPrintable(toolkit->name),
                qPrintable(gf::core::commandFor(*toolkit, QDir(jobDir).absolutePath()).join(' ')));
    std::fflush(stdout);

    // The same runner as the window's. Ctrl+C cancels: the toolkit is asked
    // to stop (SIGTERM, toolkits spec 5), and killed 10 s later if not.
    struct sigaction interrupt = {};
    interrupt.sa_handler = onInterrupt;
    sigaction(SIGINT, &interrupt, nullptr);
    gf::core::JobRunner runner;
    QEventLoop loop;
    int code = 1;
    QString result;
    QObject::connect(&runner, &gf::core::JobRunner::line, [](const gf::core::ProtocolLine &l) {
        using Type = gf::core::ProtocolLine::Type;
        if (l.type == Type::Progress)
            std::printf("  %3.0f%%  %s\n", 100 * l.fraction, qPrintable(l.text));
        else if (l.type == Type::Metric)
            std::printf("        %s: %g (%s)\n", qPrintable(l.name), l.value, qPrintable(l.text));
        else if (l.type == Type::Log)
            std::printf("  %s: %s\n", qPrintable(l.level), qPrintable(l.text));
        else if (l.type == Type::Other)
            std::printf("  %s\n", qPrintable(l.text));  // kept, as the spec says: anything else is the log
        std::fflush(stdout);
    });
    QObject::connect(&runner, &gf::core::JobRunner::finished, [&](int exit, const QString &results, bool) {
        code = exit;
        result = results;
        loop.quit();
    });
    QTimer watchInterrupt;
    QObject::connect(&watchInterrupt, &QTimer::timeout, [&] {
        if (interrupted && runner.running()) {
            interrupted = 0;
            std::printf("  cancelling: %s asked to stop\n", qPrintable(toolkit->name));
            std::fflush(stdout);
            runner.stop();
        }
    });
    watchInterrupt.start(100);
    if (!runner.start(*toolkit, jobDir, &why)) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(why));
        return 1;
    }
    loop.exec();
    if (code == 3) {
        std::printf("cancelled; the job's files are kept in %s\n", qPrintable(QDir(jobDir).absolutePath()));
        return 3;
    }
    if (code != 0 || result.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s stopped with exit code %d\n", qPrintable(toolkit->name), code);
        return 1;
    }
    QFile reportFile(QDir(jobDir).filePath("results/results.json"));
    reportFile.open(QIODevice::ReadOnly);
    const QJsonObject report = QJsonDocument::fromJson(reportFile.readAll()).object();
    std::printf("results: %s (%s by %s)\n", qPrintable(result), qPrintable(report["trust"].toString()),
                qPrintable(report["solver"].toString()));
    if (resultsOut)
        *resultsOut = result;
    return 0;
}

// How far the results can be trusted against the mesh (core/Convergence):
// the set-up solved on three meshes, each finer than the last by `ratio`
// in every size the recipe makes, and the largest displacement and the
// largest von Mises stress compared.
int converge(const QString &setupFile, const QString &part, const QString &size, const QString &order,
             const QString &throughWall, const QString &outDir, const QString &ratioText)
{
    if (outDir.isEmpty()) {
        std::fprintf(stderr, "gf-study: converge needs -o DIR, the folder for its three jobs\n");
        return 1;
    }
    bool ratioOk = true;
    const double ratio = ratioText.isEmpty() ? 1.5 : ratioText.toDouble(&ratioOk);
    if (!ratioOk || !(ratio >= 1.3)) {
        // Celik et al. (2008): a ratio of at least 1.3, for the meshes'
        // differences to stand clear of other errors.
        std::fprintf(stderr, "gf-study: --ratio is 1.3 or more, not %s\n", qPrintable(ratioText));
        return 1;
    }
    struct Solved {
        double finer;
        vtkIdType elements, nodes;
        gf::core::ResultPeaks peaks;
    };
    std::vector<Solved> solved;
    gf::core::Setup setup;
    for (int k = 0; k < 3; ++k) {
        const double finer = std::pow(ratio, k);
        QJsonObject setupJson;
        gf::core::ResultsSeries mesh;
        std::printf("mesh %d of 3: every size over %s\n", k + 1, qPrintable(QString::number(finer, 'g', 4)));
        std::fflush(stdout);
        if (!prepare(setupFile, part, size, order, throughWall, &setup, &setupJson, &mesh, finer))
            return 1;
        const QString job = QDir(outDir).filePath(QString("mesh-%1").arg(k + 1));
        QString results;
        if (const int code = solvePrepared(setupFile, setup, setupJson, mesh, job, &results); code != 0)
            return code;
        const gf::core::ResultPeaks peaks = gf::core::resultPeaks(results);
        if (!peaks.error.isEmpty()) {
            std::fprintf(stderr, "gf-study: %s\n", qPrintable(peaks.error));
            return 1;
        }
        vtkUnstructuredGrid *g = mesh.points.front().grid;
        solved.push_back({finer, g->GetNumberOfCells(), g->GetNumberOfPoints(), peaks});
    }
    const bool mm = setup.units == gf::core::Units::MillimetreNewtonSecond;
    const QString length = mm ? "mm" : "m", stress = mm ? "MPa" : "Pa";
    QJsonArray meshes;
    std::printf("\nmesh convergence (ratio %s; Celik et al. 2008, grid convergence index):\n",
                qPrintable(QString::number(ratio)));
    for (const Solved &s : solved) {
        std::printf("  every size over %-6s %8lld elements: largest displacement %s %s, largest von Mises %s\n",
                    qPrintable(QString::number(s.finer, 'g', 4)), static_cast<long long>(s.elements),
                    qPrintable(QString::number(s.peaks.displacement, 'g', 6)), qPrintable(length),
                    s.peaks.vonMises < 0 ? "none"
                                         : qPrintable(QString::number(s.peaks.vonMises, 'g', 6) + " " + stress));
        meshes.append(QJsonObject{{"finer", s.finer}, {"elements", double(s.elements)}, {"nodes", double(s.nodes)},
                                  {"largest displacement", s.peaks.displacement},
                                  {"largest von Mises stress", s.peaks.vonMises}});
    }
    // The ratios the meshes really have, from their element counts: Netgen
    // grades and rounds, so they are not the ratio asked for exactly.
    const double rCoarse = gf::core::refinementRatio(solved[0].elements, solved[1].elements);
    const double rFine = gf::core::refinementRatio(solved[1].elements, solved[2].elements);
    std::printf("  element sizes' ratios, from the counts: %s, then %s\n",
                qPrintable(QString::number(rCoarse, 'f', 3)), qPrintable(QString::number(rFine, 'f', 3)));
    QJsonObject verdicts;
    auto judge = [&](const char *quantity, const QString &unit, double a, double b, double c) {
        const QString words = gf::core::convergenceWords(quantity, unit, a, b, c, rCoarse, rFine);
        std::printf("  %s\n", qPrintable(words));
        verdicts[quantity] = words;
    };
    judge("largest displacement", length, solved[0].peaks.displacement, solved[1].peaks.displacement,
          solved[2].peaks.displacement);
    if (solved[0].peaks.vonMises >= 0)
        judge("largest von Mises stress", stress, solved[0].peaks.vonMises, solved[1].peaks.vonMises,
              solved[2].peaks.vonMises);
    QSaveFile report(QDir(outDir).filePath("convergence.json"));
    const QJsonObject json{{"ratio asked", ratio},
                           {"ratios from the element counts", QJsonArray{rCoarse, rFine}},
                           {"method", "Richardson extrapolation and the grid convergence index (safety factor "
                                      "1.25), as Celik et al. 2008, ASME J. Fluids Eng. 130(7) 078001; the order "
                                      "from f = f0 + C h^p with h from the element counts"},
                           {"meshes", meshes},
                           {"verdicts", verdicts}};
    if (!report.open(QIODevice::WriteOnly) || report.write(QJsonDocument(json).toJson()) < 0 || !report.commit()) {
        std::fprintf(stderr, "gf-study: cannot write %s\n", qPrintable(report.fileName()));
        return 1;
    }
    std::printf("wrote %s\n", qPrintable(QDir(outDir).absoluteFilePath("convergence.json")));
    return 0;
}

int deck(const QString &setupFile, const QString &part, const QString &size, const QString &order,
         const QString &throughWall, const QString &outDir)
{
    gf::core::Setup setup;
    QJsonObject setupJson;
    gf::core::ResultsSeries mesh;
    if (!prepare(setupFile, part, size, order, throughWall, &setup, &setupJson, &mesh))
        return 1;
    QString error;
    const QString text = error.isEmpty() ? gf::calculix::analysisInp(setup, mesh, "mesh.inp", &error) : QString();
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    const QString dir = outDir.isEmpty() ? QFileInfo(setupFile).absolutePath() : outDir;
    QDir().mkpath(dir);
    const QString meshPath = QDir(dir).filePath("mesh.inp");
    const QString deckPath = QDir(dir).filePath(QFileInfo(setupFile).completeBaseName() + ".inp");
    QSaveFile out(deckPath);
    if (!gf::calculix::writeMeshInp(mesh, meshPath, &error) || !out.open(QIODevice::WriteOnly)
        || out.write(text.toUtf8()) < 0 || !out.commit()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error.isEmpty() ? "cannot write " + deckPath : error));
        return 1;
    }
    std::printf("wrote %s (the mesh: %lld nodes, %lld elements, with the set-up's groups as sets)\n",
                qPrintable(meshPath), static_cast<long long>(mesh.points.front().grid->GetNumberOfPoints()),
                static_cast<long long>(mesh.points.front().grid->GetNumberOfCells()));
    std::printf("wrote %s (the job's deck: %zu material%s, %zu step%s)\n", qPrintable(deckPath),
                setup.materials.size(), setup.materials.size() == 1 ? "" : "s", setup.steps.size(),
                setup.steps.size() == 1 ? "" : "s");
    return 0;
}

int convert(const QString &input, const QString &outDir, QString deck, bool pvd)
{
    const QFileInfo in(input);
    const gf::calculix::FrdFile frd = gf::calculix::readFrd(input);
    if (!frd.ok()) {
        std::fprintf(stderr, "gf-study: %s: %s\n", qPrintable(input), qPrintable(frd.error));
        return 1;
    }
    QString error;
    gf::core::ResultsSeries series = gf::calculix::toSeries(frd, in.fileName(), &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s: %s\n", qPrintable(input), qPrintable(error));
        return 1;
    }

    // The named sets live in the deck, not in the results.
    QString sets;
    const bool named = !deck.isEmpty();
    if (!named)
        deck = in.absoluteDir().filePath(in.completeBaseName() + ".inp");
    const QString deckName = QFileInfo(deck).fileName();
    if (pvd) {
        sets = ", no sets: a .vtu series has no blocks";
    } else if (!named && !QFileInfo::exists(deck)) {
        sets = QString(", no sets: no %1 beside it").arg(deckName);
    } else {
        const gf::calculix::InpSets read = gf::calculix::readInpSets(deck);
        if (!read.ok()) {
            std::fprintf(stderr, "gf-study: %s\n", qPrintable(read.error));
            return 1;
        }
        gf::calculix::addSets(series, read, deckName);
        sets = QString(", %1 set%2 from %3")
                   .arg(read.sets.size())
                   .arg(read.sets.size() == 1 ? "" : "s", deckName);
    }

    const QString written = write(series, in, outDir, pvd, &error);
    if (written.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    const size_t n = series.points.size();
    std::printf("wrote %s (%zu time point%s, %zu nodes, %zu elements%s)\n", qPrintable(written), n,
                n == 1 ? "" : "s", frd.nodes.size(), frd.elements.size(), qPrintable(sets));
    for (const QString &line : series.provenance)
        std::printf("  %s\n", qPrintable(line));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("gf-study"));
    QCoreApplication::setApplicationVersion(gf::core::applicationVersion());

    QCommandLineParser parser;
    parser.setApplicationDescription(gf::core::applicationName() + ", the " + gf::core::applicationSubtitle()
                                     + ", without a window.\n\n"
                                     "Commands:\n"
                                     "  convert job.frd   CalculiX results to one .vtkhdf for "
                                     "ParaView, the deck's named sets as blocks\n"
                                     "  convert mesh.msh  a Gmsh mesh likewise, its physical "
                                     "groups as the sets\n"
                                     "  mesh part.step    the part's solids meshed with "
                                     "tetrahedra, each solid and face as a set\n"
                                     "  deck setup.json part.step   a CalculiX job from a set-up: "
                                     "mesh.inp and the deck around it\n"
                                     "  solve setup.json part.step -o JOB   the set-up solved by "
                                     "its toolkit, results in JOB/results\n"
                                     "  converge setup.json part.step -o DIR   the set-up solved "
                                     "on three meshes, each finer by --ratio, and how far its "
                                     "results can be trusted against the mesh\n"
                                     "  mesh-worker part.step --recipe JSON -o mesh.vtu   what the "
                                     "window runs to mesh, in a process of its own\n\n"
                                     "A part is a STEP file or a .brep (as FreeCAD's exportBrep "
                                     "writes it, in mm). A set-up is a study.json, or a file "
                                     "holding its \"setup\".");
    parser.addHelpOption();
    const QCommandLineOption versionOpt("version", "Print the version and build.");
    const QCommandLineOption outOpt({"o", "out"},
                                    "Write into DIR (default: beside the input).", "DIR");
    const QCommandLineOption deckOpt("deck",
                                     "Read the named sets from FILE (default: job.inp beside "
                                     "the results, if there is one).", "FILE");
    const QCommandLineOption pvdOpt("pvd",
                                    "Write a .vtu series and its .pvd instead (no sets).");
    parser.addOption(versionOpt);
    parser.addOption(outOpt);
    parser.addOption(deckOpt);
    const QCommandLineOption sizeOpt("size", "mesh: the largest element edge, in the "
                                             "geometry's units.", "H");
    const QCommandLineOption orderOpt("order", "mesh: 1 (straight edges, the default) or 2 "
                                               "(a middle node on each edge, on the geometry).",
                                      "N");
    const QCommandLineOption inpOpt("inp", "mesh: also write the mesh and its sets as "
                                           "<name>.inp, for a CalculiX deck to *INCLUDE.");
    parser.addOption(pvdOpt);
    parser.addOption(inpOpt);
    parser.addOption(sizeOpt);
    parser.addOption(orderOpt);
    const QCommandLineOption wallOpt("through-wall", "mesh: elements through a thin wall (default 2; 0: none "
                                                     "added). A wall (a face at least four times as wide as "
                                                     "the wall behind it is thick) is meshed at its thickness "
                                                     "over N + 1, where that is finer than --size.",
                                     "N");
    parser.addOption(wallOpt);
    const QCommandLineOption recipeOpt("recipe", "mesh-worker: the mesh step's parameters, as study.json "
                                                 "keeps them (JSON).", "JSON");
    parser.addOption(recipeOpt);
    const QCommandLineOption ratioOpt("ratio", "converge: how much finer each mesh is than the last (default "
                                               "1.5; at least 1.3).", "R");
    parser.addOption(ratioOpt);
    parser.addPositionalArgument("command", "convert, mesh, deck or solve");
    parser.addPositionalArgument("file", "The input file.");
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        std::printf("%s %s (build %s)\n", qPrintable(gf::core::applicationName()),
                    qPrintable(gf::core::applicationVersion()),
                    qPrintable(gf::core::applicationBuild()));
        return 0;
    }
    const QStringList args = parser.positionalArguments();
    if (args.size() == 2 && args[0] == "convert" && args[1].endsWith(".msh", Qt::CaseInsensitive)) {
        if (parser.isSet(deckOpt)) {
            std::fprintf(stderr, "gf-study: --deck is for CalculiX results; a .msh holds "
                                 "its own sets\n");
            return 1;
        }
        return convertMesh(args[1], parser.value(outOpt), parser.isSet(pvdOpt));
    }
    if (args.size() == 3 && args[0] == "solve")
        return solve(args[1], args[2], parser.value(sizeOpt), parser.value(orderOpt), parser.value(wallOpt),
                     parser.value(outOpt));
    if (args.size() == 3 && args[0] == "deck")
        return deck(args[1], args[2], parser.value(sizeOpt), parser.value(orderOpt), parser.value(wallOpt),
                    parser.value(outOpt));
    if (args.size() == 3 && args[0] == "converge")
        return converge(args[1], args[2], parser.value(sizeOpt), parser.value(orderOpt), parser.value(wallOpt),
                        parser.value(outOpt), parser.value(ratioOpt));
    if (args.size() == 2 && args[0] == "mesh-worker")
        return meshWorker(args[1], parser.value(recipeOpt), parser.value(outOpt));
    if (args.size() == 2 && args[0] == "mesh")
        return mesh(args[1], parser.value(sizeOpt), parser.value(orderOpt), parser.value(wallOpt), parser.value(outOpt),
                    parser.isSet(pvdOpt), parser.isSet(inpOpt));
    if (args.size() == 2 && args[0] == "convert")
        return convert(args[1], parser.value(outOpt), parser.value(deckOpt),
                       parser.isSet(pvdOpt));
    if (!args.isEmpty())
        std::fprintf(stderr, "gf-study: not a command: %s\n\n", qPrintable(args.join(' ')));
    parser.showHelp(1);
}
