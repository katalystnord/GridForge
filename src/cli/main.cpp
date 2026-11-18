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
//   Both mesh commands measure the mesh: the verdict and measures on the
//   terminal, the report for agents in <name>.quality.json beside it, and
//   each cell's "mean ratio" on the grid for ParaView.

#include "calculix/FrdToVtk.h"
#include "calculix/DeckWriter.h"
#include "calculix/InpWriter.h"
#include "core/Job.h"
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

// The set-up read and filled from its cards, and the part meshed with its
// groups as sets: what both deck and solve start from.
bool prepare(const QString &setupFile, const QString &part, const QString &size, const QString &order,
             const QString &throughWall,
             gf::core::Setup *setup, QJsonObject *setupJson, gf::core::ResultsSeries *mesh)
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
    *mesh = gf::mesh::meshStep(part, recipe, &error);
    if (error.isEmpty())
        gf::mesh::addGroupSets(*mesh, *setup, &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return false;
    }
    return true;
}

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
                                     "its toolkit, results in JOB/results\n\n"
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
