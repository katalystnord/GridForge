cmake_minimum_required(VERSION 3.21)
# The name is a working name (David, 2026-11-14: still to decide). It is
# said once here and once in src/core/ApplicationInfo.cpp.
project(FluidStress VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for editors (clangd); not used by the build

set(CMAKE_AUTOMOC ON)

# Address and undefined-behaviour sanitizers, in their own build directory
# (tools/sanitize.sh): the instrumentation changes the object code.
option(FS_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)
if(FS_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                        -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# Qt: only LGPL modules. Qt Charts, Qt Graphs, Qt Quick 3D and Qt Data
# Visualization are GPL-3.0 only and would force the project's licence.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Test)

# Ubuntu's VTK re-exports system libraries through its imported targets but
# leaves the consumer to locate them (learned in SurView and TenForge). MPI's
# C target needs LANGUAGES C above.
find_package(MPI QUIET)
find_package(jsoncpp QUIET)

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    IOXML                     # .vtu/.pvd out, for ParaView and PyVista
    RenderingCore
    RenderingOpenGL2
    InteractionStyle
    GUISupportQt              # QVTKOpenGLNativeWidget
)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
