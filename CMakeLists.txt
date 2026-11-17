cmake_minimum_required(VERSION 3.21)
# Named GridForge by David, 2026-11-15 (working name FluidStress). It is
# said once here and once in src/core/ApplicationInfo.cpp.
project(GridForge VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for editors (clangd); not used by the build

set(CMAKE_AUTOMOC ON)

# Address and undefined-behaviour sanitizers, in their own build directory
# (tools/sanitize.sh): the instrumentation changes the object code.
option(GF_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)
if(GF_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                        -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# Qt: only LGPL modules. Qt Charts, Qt Graphs, Qt Quick 3D and Qt Data
# Visualization are GPL-3.0 only and would force the project's licence.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Test)

# Ubuntu's VTK re-exports system libraries through its imported targets but
# leaves the consumer to locate them (learned in SurView and TenSense). MPI's
# C target needs LANGUAGES C above.
find_package(MPI QUIET)
find_package(jsoncpp QUIET)

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    CommonExecutionModel      # the time series as a pipeline source, for the VTKHDF writer
    IOXML                     # .vtu/.pvd out, for ParaView and PyVista
    IOHDF                     # .vtkhdf out: one file, every time point, sets as blocks
    RenderingCore
    RenderingOpenGL2
    InteractionStyle
    GUISupportQt              # QVTKOpenGLNativeWidget
)

# Netgen (LGPL-2.1), GridForge's meshing engine: built from our fork in
# third_party/netgen at the commit the submodule pins, by
# tools/build-netgen.sh, into build-netgen/install. OpenCASCADE (LGPL-2.1
# with its header exception) comes with it: Netgen reads STEP through it.
find_package(Netgen CONFIG REQUIRED
    PATHS ${CMAKE_SOURCE_DIR}/build-netgen/install NO_DEFAULT_PATH)
if(NOT NETGEN_USE_OCC)
    message(FATAL_ERROR "Netgen was built without OpenCASCADE; run tools/build-netgen.sh -f")
endif()
# The commit built, for the provenance; configure runs again when it changes.
set(GF_NETGEN_STAMP ${CMAKE_SOURCE_DIR}/build-netgen/install/.gridforge-netgen-commit)
file(STRINGS ${GF_NETGEN_STAMP} GF_NETGEN_COMMIT LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GF_NETGEN_STAMP})

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
