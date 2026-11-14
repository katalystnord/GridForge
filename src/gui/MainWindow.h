#pragma once

#include <QMainWindow>

class QVTKOpenGLNativeWidget;

namespace fs::gui {

// The application window. It binds widgets to fs_core and holds no logic of
// its own (headless core, thin GUI).
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QVTKOpenGLNativeWidget *m_view = nullptr;
};

} // namespace fs::gui
