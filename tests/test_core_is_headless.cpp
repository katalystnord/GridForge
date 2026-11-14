// The core alone: built against fs_core's own link line (Qt Core, VTK's data
// model), which carries no Qt Widgets and no VTK rendering. If a core header
// reached for either, this file would not compile.

#include "core/ApplicationInfo.h"

#include <cstdio>

int main()
{
    if (fs::core::applicationName().isEmpty() || fs::core::applicationVersion().isEmpty()) {
        std::fprintf(stderr, "the core does not say what it is\n");
        return 1;
    }
    return 0;
}
