#include "core/ApplicationInfo.h"

namespace gf::core {

// Named by David, 2026-11-15 (working name FluidStress).
QString applicationName()
{
    return QStringLiteral("GridForge");
}

QString applicationSubtitle()
{
    return QStringLiteral("open simulation set-up between FreeCAD and ParaView");
}

QString applicationVersion()
{
    return QStringLiteral(GF_VERSION);
}

QString applicationBuild()
{
    return QStringLiteral(GF_BUILD);
}

} // namespace gf::core
