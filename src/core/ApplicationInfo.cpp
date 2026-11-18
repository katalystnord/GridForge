#include "core/ApplicationInfo.h"

namespace gf::core {

extern const char *buildStamp;  // BuildStamp.cpp, generated at every build

// Named by David, 2026-11-15 (working name FluidStress).
QString applicationName()
{
    return QStringLiteral("GridForge");
}

QString applicationSubtitle()
{
    return QStringLiteral("meshing and simulation workbench");
}

QString applicationVersion()
{
    return QStringLiteral(GF_VERSION);
}

QString applicationBuild()
{
    return QString::fromLatin1(buildStamp);
}

} // namespace gf::core
