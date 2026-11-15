// The core alone: built against gf_core's own link line (Qt Core, VTK's data
// model), which carries no Qt Widgets and no VTK rendering. If a core header
// reached for either, this file would not compile.

#include "core/ApplicationInfo.h"

#include <cstdio>

int main()
{
    if (gf::core::applicationName().isEmpty() || gf::core::applicationVersion().isEmpty()) {
        std::fprintf(stderr, "the core does not say what it is\n");
        return 1;
    }
    return 0;
}
