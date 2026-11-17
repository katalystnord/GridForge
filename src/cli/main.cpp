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
#include <QProcess>
#include <QSaveFile>

#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>

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

int mesh(const QString &input, const QString &size, const QString &order, const QString &outDir,
         bool pvd, bool inp)
{
    bool sizeOk = false, orderOk = false;
    const gf::mesh::Recipe recipe{size.toDouble(&sizeOk), order.isEmpty() ? 1 : order.toInt(&orderOk)};
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
    const gf::mesh::Recipe recipe{size.toDouble(&sizeOk), order.isEmpty() ? 1 : order.toInt(&orderOk)};
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

int solve(const QString &setupFile, const QString &part, const QString &size, const QString &order,
          const QString &jobDir)
{
    if (jobDir.isEmpty()) {
        std::fprintf(stderr, "gf-study: solve needs -o JOB, the job folder to write\n");
        return 1;
    }
    gf::core::Setup setup;
    QJsonObject setupJson;
    gf::core::ResultsSeries mesh;
    if (!prepare(setupFile, part, size, order, &setup, &setupJson, &mesh))
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
    const gf::core::ToolkitSet toolkits = gf::core::findToolkits({GF_SHIPPED_TOOLKITS});
    std::vector<gf::core::Offer> offers = toolkits.offers("solve", "structural.static", {});
    const gf::core::Toolkit *toolkit = nullptr;
    for (const gf::core::Offer &o : offers)
        if (o.can && !toolkit && gf::core::availability(*o.toolkit).ready)
            toolkit = o.toolkit;
    if (!toolkit) {
        std::fprintf(stderr, "gf-study: no solve toolkit for structural.static is ready\n");
        for (const gf::core::Offer &o : offers)
            std::fprintf(stderr, "  %s: %s\n", qPrintable(o.toolkit->name),
                         qPrintable(o.can ? "missing " + gf::core::availability(*o.toolkit).missing.join(", ") : o.why));
        return 1;
    }
    const QStringList command = gf::core::commandFor(*toolkit, QDir(jobDir).absolutePath());
    std::printf("solving with %s (%s)\n", qPrintable(toolkit->name), qPrintable(command.join(' ')));
    std::fflush(stdout);

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(command.first(), command.mid(1));
    if (!process.waitForStarted(10000)) {
        std::fprintf(stderr, "gf-study: %s did not start\n", qPrintable(command.first()));
        return 1;
    }
    QByteArray pending;
    QString result;
    auto show = [&](const QByteArray &line) {
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        const QString type = o["type"].toString();
        if (type == "progress")
            std::printf("  %3.0f%%  %s\n", 100 * o["fraction"].toDouble(), qPrintable(o["text"].toString()));
        else if (type == "metric")
            std::printf("        %s: %g (%s)\n", qPrintable(o["name"].toString()), o["value"].toDouble(),
                        qPrintable(o["text"].toString()));
        else if (type == "log")
            std::printf("  %s: %s\n", qPrintable(o["level"].toString()), qPrintable(o["text"].toString()));
        else if (type == "file")
            result = o["path"].toString();
        else if (!line.trimmed().isEmpty())
            std::printf("  %s\n", line.constData());  // kept, as the spec says: anything else is the log
        std::fflush(stdout);
    };
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(200);
        pending += process.readAllStandardOutput();
        qsizetype nl;
        while ((nl = pending.indexOf('\n')) >= 0) {
            show(pending.left(nl));
            pending.remove(0, nl + 1);
        }
    }
    pending += process.readAllStandardOutput();
    for (const QByteArray &line : pending.split('\n'))
        show(line);
    if (process.exitCode() != 0 || result.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s stopped with exit code %d\n", qPrintable(toolkit->name), process.exitCode());
        return 1;
    }
    QFile reportFile(QDir(jobDir).filePath("results/results.json"));
    reportFile.open(QIODevice::ReadOnly);
    const QJsonObject report = QJsonDocument::fromJson(reportFile.readAll()).object();
    std::printf("results: %s (%s by %s)\n", qPrintable(QDir(jobDir).absoluteFilePath(result)),
                qPrintable(report["trust"].toString()), qPrintable(report["solver"].toString()));
    return 0;
}

int deck(const QString &setupFile, const QString &part, const QString &size, const QString &order,
         const QString &outDir)
{
    gf::core::Setup setup;
    QJsonObject setupJson;
    gf::core::ResultsSeries mesh;
    if (!prepare(setupFile, part, size, order, &setup, &setupJson, &mesh))
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
    parser.setApplicationDescription(gf::core::applicationName() + ", without a window: "
                                     + gf::core::applicationSubtitle() + ".\n\n"
                                     "Commands:\n"
                                     "  convert job.frd   CalculiX results to one .vtkhdf for "
                                     "ParaView, the deck's named sets as blocks\n"
                                     "  convert mesh.msh  a Gmsh mesh likewise, its physical "
                                     "groups as the sets\n"
                                     "  mesh part.step    the STEP's solids meshed with "
                                     "tetrahedra, each solid and face as a set\n"
                                     "  deck setup.json part.step   a CalculiX job from a set-up: "
                                     "mesh.inp and the deck around it\n"
                                     "  solve setup.json part.step -o JOB   the set-up solved by "
                                     "its toolkit, results in JOB/results");
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
    parser.addPositionalArgument("command", "convert or mesh");
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
        return solve(args[1], args[2], parser.value(sizeOpt), parser.value(orderOpt), parser.value(outOpt));
    if (args.size() == 3 && args[0] == "deck")
        return deck(args[1], args[2], parser.value(sizeOpt), parser.value(orderOpt), parser.value(outOpt));
    if (args.size() == 2 && args[0] == "mesh")
        return mesh(args[1], parser.value(sizeOpt), parser.value(orderOpt), parser.value(outOpt),
                    parser.isSet(pvdOpt), parser.isSet(inpOpt));
    if (args.size() == 2 && args[0] == "convert")
        return convert(args[1], parser.value(outOpt), parser.value(deckOpt),
                       parser.isSet(pvdOpt));
    if (!args.isEmpty())
        std::fprintf(stderr, "gf-study: not a command: %s\n\n", qPrintable(args.join(' ')));
    parser.showHelp(1);
}
