#include "core/ApplicationInfo.h"
#include "gui/MainWindow.h"
#include "gui/Style.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
    // Answered before any window system is touched, so a package or a
    // script can ask on a machine with no display.
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--version")) {
            std::printf("%s %s (build %s)\n", qPrintable(gf::core::applicationName()),
                        qPrintable(gf::core::applicationVersion()),
                        qPrintable(gf::core::applicationBuild()));
            return 0;
        }
    }

    // Must precede QApplication: QVTKOpenGLNativeWidget needs its own default
    // surface format to get a usable OpenGL context.
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    gf::gui::applyStyle(app);
    QApplication::setApplicationName(gf::core::applicationName());
    QApplication::setOrganizationName(QStringLiteral("Katalyst Nord"));
    QApplication::setApplicationVersion(gf::core::applicationVersion());

    Q_INIT_RESOURCE(icons);  // the icons live in the static gf_gui library

    gf::gui::MainWindow window;
    window.show();
    // A study folder named on the command line is opened.
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        window.openStudy(args.last());
    return app.exec();
}
