#pragma once

#include <QString>

// The application's name and what it is, said once. The window title, the
// command line and every file the application writes take them from here.

namespace gf::core {

QString applicationName();
QString applicationSubtitle();
QString applicationVersion();
QString applicationBuild();  // git describe at build time; "unknown" outside git

} // namespace gf::core
