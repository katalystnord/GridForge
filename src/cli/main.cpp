// fs-study: the application's work without a window, for scripts, CI and
// the test suite. In this skeleton it answers --version only.

#include "core/ApplicationInfo.h"

#include <QCommandLineParser>
#include <QCoreApplication>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("fs-study"));
    QCoreApplication::setApplicationVersion(fs::core::applicationVersion());

    QCommandLineParser parser;
    parser.setApplicationDescription(fs::core::applicationName() + ", without a window: "
                                     + fs::core::applicationSubtitle() + ".");
    parser.addHelpOption();
    const QCommandLineOption versionOpt("version", "Print the version and build.");
    parser.addOption(versionOpt);
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        std::printf("%s %s (build %s)\n", qPrintable(fs::core::applicationName()),
                    qPrintable(fs::core::applicationVersion()),
                    qPrintable(fs::core::applicationBuild()));
        return 0;
    }
    parser.showHelp(1);
}
