#include "gui/MainWindow.h"

#include "core/ApplicationInfo.h"

#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkRenderer.h>

namespace fs::gui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(core::applicationName() + " - " + core::applicationSubtitle());
    m_view = new QVTKOpenGLNativeWidget(this);
    m_view->setObjectName(QStringLiteral("view"));
    vtkNew<vtkGenericOpenGLRenderWindow> window;
    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(1.0, 1.0, 1.0);
    window->AddRenderer(renderer);
    m_view->setRenderWindow(window);
    setCentralWidget(m_view);
    resize(1200, 800);
}

} // namespace fs::gui
