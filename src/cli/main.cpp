// gf-study: the application's work without a window, for scripts, CI and
// the test suite.
//
//   gf-study --version
//   gf-study convert job.frd [-o DIR]   results to a .pvd series for ParaView

#include "calculix/FrdToVtk.h"
#include "core/ApplicationInfo.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>

#include <cstdio>

namespace {

int convert(const QString &input, const QString &outDir)
{
    const QFileInfo in(input);
    const gf::calculix::FrdFile frd = gf::calculix::readFrd(input);
    if (!frd.ok()) {
        std::fprintf(stderr, "gf-study: %s: %s\n", qPrintable(input), qPrintable(frd.error));
        return 1;
    }
    QString error;
    const gf::core::ResultsSeries series = gf::calculix::toSeries(frd, in.fileName(), &error);
    if (!error.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s: %s\n", qPrintable(input), qPrintable(error));
        return 1;
    }
    const QString dir = outDir.isEmpty() ? in.absolutePath() : outDir;
    const QStringList written =
        gf::core::writeVtuSeries(series, dir, in.completeBaseName(), &error);
    if (written.isEmpty()) {
        std::fprintf(stderr, "gf-study: %s\n", qPrintable(error));
        return 1;
    }
    std::printf("wrote %s (%zu time points, %zu nodes, %zu elements)\n",
                qPrintable(written.first()), series.points.size(), frd.nodes.size(),
                frd.elements.size());
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
                                     "  convert job.frd   CalculiX results to a .pvd series "
                                     "(.vtu files) for ParaView");
    parser.addHelpOption();
    const QCommandLineOption versionOpt("version", "Print the version and build.");
    const QCommandLineOption outOpt({"o", "out"},
                                    "Write into DIR (default: beside the input).", "DIR");
    parser.addOption(versionOpt);
    parser.addOption(outOpt);
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
        return convert(args[1], parser.value(outOpt));
    if (!args.isEmpty())
        std::fprintf(stderr, "gf-study: not a command: %s\n\n", qPrintable(args.join(' ')));
    parser.showHelp(1);
}
