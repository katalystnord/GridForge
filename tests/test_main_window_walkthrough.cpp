// The window, walked through as a user would (layout A of
// docs/survey/gui-layouts.md): the dogbone study opened, the tree, the
// properties, the mesh made in the background into the view. On a private
// X server, never the desktop (TenSense's ensureHeadless, below).
//
//   test_main_window_walkthrough [--screenshot FILE]   also saves the window

#include "gui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <vector>

#ifndef GF_EXAMPLES
#error "GF_EXAMPLES must point at examples/"
#endif

using gf::gui::MainWindow;

namespace {
QString screenshot;  // --screenshot FILE

QStringList topLevel(QTreeWidget *t)
{
    QStringList out;
    for (int i = 0; i < t->topLevelItemCount(); ++i)
        out << t->topLevelItem(i)->text(0);
    return out;
}

QTreeWidgetItem *findItem(QTreeWidget *t, const QString &title)
{
    const auto found = t->findItems(title, Qt::MatchExactly | Qt::MatchRecursive);
    return found.isEmpty() ? nullptr : found.first();
}

QString propertyValue(QWidget *properties, const QString &label)
{
    auto *form = qobject_cast<QFormLayout *>(properties->layout());
    for (int r = 0; form && r < form->rowCount(); ++r) {
        auto *l = qobject_cast<QLabel *>(form->itemAt(r, QFormLayout::LabelRole)->widget());
        auto *v = qobject_cast<QLabel *>(form->itemAt(r, QFormLayout::FieldRole)->widget());
        if (l && v && l->text() == label)
            return v->text();
    }
    return {};
}
} // namespace

class MainWindowWalkthrough : public QObject
{
    Q_OBJECT
private slots:
    void the_window_has_layout_a()
    {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (const char *name : {"studyDock", "propertiesDock", "informationDock", "messagesDock", "jobDock",
                                 "stepsDock", "deckDock"})
            QVERIFY2(w.findChild<QDockWidget *>(name), name);
        // Study over Properties on the left; Properties and Information tabbed.
        auto *study = w.findChild<QDockWidget *>("studyDock");
        auto *properties = w.findChild<QDockWidget *>("propertiesDock");
        QCOMPARE(w.dockWidgetArea(study), Qt::LeftDockWidgetArea);
        QCOMPARE(w.dockWidgetArea(properties), Qt::LeftDockWidgetArea);
        QVERIFY(study->geometry().bottom() <= properties->geometry().top() + 5);
        QVERIFY(w.tabifiedDockWidgets(properties).contains(w.findChild<QDockWidget *>("informationDock")));
        QCOMPARE(w.dockWidgetArea(w.findChild<QDockWidget *>("jobDock")), Qt::BottomDockWidgetArea);
        for (const char *bar : {"mainToolbar", "runToolbar", "cameraToolbar", "vcrToolbar", "stepToolbar"})
            QVERIFY2(w.findChild<QToolBar *>(bar), bar);
        // ParaView's icons are there (the resource is in the binary).
        auto *reset = w.findChild<QAction *>("resetCamera");
        QVERIFY(reset && !reset->icon().isNull());
        QVERIFY(!reset->icon().pixmap(24, 24).isNull());
    }

    void opening_the_dogbone_fills_the_tree_and_the_units()
    {
        MainWindow w;
        w.show();
        QVERIFY(w.openStudy(QString(GF_EXAMPLES) + "/dogbone-plastic"));
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        QCOMPARE(topLevel(tree), QStringList({"Geometry", "Mesh", "Materials", "Groups", "Supports", "Steps", "Jobs",
                                              "Results"}));
        QTreeWidgetItem *load = findItem(tree, "Displacement: Pulled grip");
        QVERIFY(load);
        QCOMPARE(load->text(1), QString("x 1.5, y 0, z 0 mm"));
        QVERIFY(!load->icon(0).isNull());  // FreeCAD's FEM icon
        QCOMPARE(w.findChild<QLabel *>("units")->text(), QString("mm, N, s, MPa"));
        QCOMPARE(w.findChild<QComboBox *>("stepChooser")->currentText(), QString("Pull"));
        QVERIFY(w.windowTitle().contains("dogbone-plastic"));
    }

    void selecting_an_item_shows_it_in_properties()
    {
        MainWindow w;
        w.show();
        w.openStudy(QString(GF_EXAMPLES) + "/dogbone-plastic");
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        tree->setCurrentItem(findItem(tree, "Measured steel"));
        auto *properties = w.findChild<QWidget *>("properties");
        QCOMPARE(propertyValue(properties, "Name"), QString("Measured steel"));
        QCOMPARE(propertyValue(properties, "Value"), QString("card tensense-ro.FCMat, plastic"));
    }

    void the_mesh_is_made_without_freezing_the_window_and_shown()
    {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        // While it meshes, the window's own event loop keeps running: a
        // timer set before meshing fires during it (tenet 7).
        int ticks = 0;
        QTimer tick;
        connect(&tick, &QTimer::timeout, [&] { ++ticks; });
        tick.start(20);
        QElapsedTimer clock;
        clock.start();
        QVERIFY(w.openStudy(QString(GF_EXAMPLES) + "/dogbone-plastic"));
        QVERIFY(w.meshing());
        QTRY_VERIFY_WITH_TIMEOUT(!w.meshing(), 120000);
        QVERIFY2(ticks > 5, qPrintable(QString("only %1 ticks in %2 ms").arg(ticks).arg(clock.elapsed())));
        auto *tree = w.findChild<QTreeWidget *>("studyTree");
        QTreeWidgetItem *mesh = findItem(tree, "Mesh");
        QVERIFY(mesh);
        QCOMPARE(mesh->text(1), QString("1624 tetrahedra, 3575 nodes; ready for a solver"));
        auto *view = w.findChild<QVTKOpenGLNativeWidget *>("view");
        vtkRenderer *r = view->renderWindow()->GetRenderers()->GetFirstRenderer();
        QCOMPARE(r->GetActors()->GetNumberOfItems(), 1);
        QVERIFY(w.findChild<QLabel *>("information")->text().startsWith("ready for a solver"));
        if (!screenshot.isEmpty()) {
            w.resize(1500, 950);
            QTest::qWait(500);
            view->renderWindow()->Render();
            QVERIFY(w.grab().save(screenshot));
        }
    }

    void a_study_that_cannot_be_read_is_said_in_messages()
    {
        MainWindow w;
        w.show();
        QTemporaryDir empty;
        QVERIFY(!w.openStudy(empty.path()));
        QVERIFY(w.findChild<QPlainTextEdit *>("messages")->toPlainText().contains(
            "Cannot open the study: no study.json in " + empty.path()));
        QVERIFY(w.statusBar()->currentMessage().startsWith("Cannot open the study"));
    }
};

// HEADLESS BY CONSTRUCTION (TenSense's lesson): the test shows real windows,
// so it puts itself on a private X server unless asked to use the desktop.
static void ensureHeadless(char *argv[])
{
    if (qEnvironmentVariableIsSet("GF_WALKTHROUGH_XVFB") || qEnvironmentVariable("GF_WALKTHROUGH_ON_DESKTOP") == "1")
        return;
    qputenv("GF_WALKTHROUGH_XVFB", "1");
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
    qunsetenv("WAYLAND_DISPLAY");
    std::vector<char *> args{const_cast<char *>("xvfb-run"), const_cast<char *>("-a"), const_cast<char *>("-s"),
                             const_cast<char *>("-screen 0 1700x1100x24")};
    for (char **a = argv; *a; ++a)
        args.push_back(*a);
    args.push_back(nullptr);
    execvp("xvfb-run", args.data());
    std::perror("walkthrough: cannot start xvfb-run (install xvfb, or set GF_WALKTHROUGH_ON_DESKTOP=1)");
    std::exit(2);
}

int main(int argc, char *argv[])
{
    ensureHeadless(argv);
    std::vector<char *> rest;
    for (int i = 0; i < argc; ++i) {
        if (QByteArray(argv[i]) == "--screenshot" && i + 1 < argc) {
            screenshot = QString::fromLocal8Bit(argv[++i]);
            continue;
        }
        rest.push_back(argv[i]);
    }
    int n = static_cast<int>(rest.size());
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
    QApplication app(n, rest.data());
    Q_INIT_RESOURCE(icons);
    MainWindowWalkthrough test;
    return QTest::qExec(&test, n, rest.data());
}

#include "test_main_window_walkthrough.moc"
