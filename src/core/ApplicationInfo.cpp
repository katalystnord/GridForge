#include "core/ApplicationInfo.h"

namespace fs::core {

// A working name (David, 2026-11-14: still to decide).
QString applicationName()
{
    return QStringLiteral("FluidStress");
}

QString applicationSubtitle()
{
    return QStringLiteral("open simulation set-up between FreeCAD and ParaView");
}

QString applicationVersion()
{
    return QStringLiteral(FS_VERSION);
}

QString applicationBuild()
{
    return QStringLiteral(FS_BUILD);
}

} // namespace fs::core
