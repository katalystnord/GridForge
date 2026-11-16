// gf-study: the application's work without a window, for scripts, CI and
// the test suite.
//
//   gf-study --version
//   gf-study convert job.frd [-o DIR] [--deck job.inp] [--pvd]
//       results to one .vtkhdf for ParaView, with the deck's named sets as
//       blocks (the deck: job.inp beside the results, unless named); with
//       --pvd, a .vtu series and its .pvd instead (no sets)

#include "calculix/FrdToVtk.h"
#include "core/ApplicationInfo.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstdio>

namespace {

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

    const QString dir = outDir.isEmpty() ? in.absolutePath() : outDir;
    if (!QDir().mkpath(dir)) {
        std::fprintf(stderr, "gf-study: cannot make the folder %s\n", qPrintable(dir));
        return 1;
    }
    QString written;
    if (pvd) {
        const QStringList files =
            gf::core::writeVtuSeries(series, dir, in.completeBaseName(), &error);
        written = files.value(0);
    } else {
        const QString path = QDir(dir).filePath(in.completeBaseName() + ".vtkhdf");
        if (gf::core::writeVtkHdf(series, path, &error))
            written = path;
    }
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
                                     "ParaView, the deck's named sets as blocks");
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
    parser.addOption(pvdOpt);
    parser.addPositionalArgument("command", "convert");
    parser.addPositionalArgument("file", "The input file.");
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        std::printf("%s %s (build %s)\n", qPrintable(gf::core::applicationName()),
                    qPrintable(gf::core::applicationVersion()),
                    qPrintable(gf::core::applicationBuild()));
        return 0;
    }
    const QStringList args = parser.positionalArguments();
    if (args.size() == 2 && args[0] == "convert")
        return convert(args[1], parser.value(outOpt), parser.value(deckOpt),
                       parser.isSet(pvdOpt));
    if (!args.isEmpty())
        std::fprintf(stderr, "gf-study: not a command: %s\n\n", qPrintable(args.join(' ')));
    parser.showHelp(1);
}
