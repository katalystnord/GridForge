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
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Concurrent Test)

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
    FiltersGeometry           # the mesh's surface for the view
    FiltersSources            # arrows and cones for supports and loads
    FiltersGeneral            # their placing (transform filter)
    FiltersCore               # all of them in one (append)
    CommonTransforms
    RenderingAnnotation       # the legend (scalar bar)
    RenderingCore
    RenderingOpenGL2
    InteractionStyle
    GUISupportQt              # QVTKOpenGLNativeWidget
)

# Netgen (LGPL-2.1), GridForge's meshing engine: built from our fork in
# third_party/netgen at the commit the submodule pins, by
# tools/build-netgen.sh, into build-netgen/install. OpenCASCADE (LGPL-2.1
# with its header exception) comes with it: Netgen reads STEP through it.
set(GF_NETGEN_PREFIX "${CMAKE_SOURCE_DIR}/build-netgen/install" CACHE PATH
    "Where tools/build-netgen.sh (or the .deb's build) installed Netgen")
find_package(Netgen CONFIG REQUIRED
    PATHS ${GF_NETGEN_PREFIX} NO_DEFAULT_PATH)
if(NOT NETGEN_USE_OCC)
    message(FATAL_ERROR "Netgen was built without OpenCASCADE; run tools/build-netgen.sh -f")
endif()
# The commit built, for the provenance; configure runs again when it changes.
set(GF_NETGEN_STAMP ${GF_NETGEN_PREFIX}/.gridforge-netgen-commit)
file(STRINGS ${GF_NETGEN_STAMP} GF_NETGEN_COMMIT LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GF_NETGEN_STAMP})

# yaml-cpp (MIT, Ubuntu's libyaml-cpp-dev): FreeCAD's material cards are
# YAML. What a card means is read by GridForge (core/MaterialCard).
find_package(yaml-cpp REQUIRED)

# Installed as the AppImage lays it out (tools/appimage): the programs in
# bin, each shipped toolkit in lib/gridforge/toolkits/<id> with its
# adapter (found there first: core::shippedToolkitFolders), and ccx beside
# the CalculiX adapter when GF_BUNDLE_CCX names the folder tools/build-ccx.sh
# installed it into (David, 2026-11-18: the AppImage bundles ccx).
set(GF_BUNDLE_CCX "" CACHE PATH "A ccx to ship with the CalculiX toolkit (tools/build-ccx.sh's build-ccx/install)")
install(FILES LICENSE NOTICE README.md DESTINATION share/doc/gridforge)
install(FILES packaging/gridforge.desktop DESTINATION share/applications)
install(FILES packaging/gridforge.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES packaging/man/GridForge.1 packaging/man/gf-study.1 DESTINATION share/man/man1)
install(FILES toolkits/calculix/toolkit.json DESTINATION lib/gridforge/toolkits/calculix)
if(GF_BUNDLE_CCX)
    install(PROGRAMS ${GF_BUNDLE_CCX}/bin/ccx DESTINATION lib/gridforge/toolkits/calculix)
    install(DIRECTORY ${GF_BUNDLE_CCX}/share/doc/ccx DESTINATION share/doc)
endif()

# Installed programs keep the paths they were linked with (Netgen's
# build-netgen/install/lib among them), so they run where installed and the
# AppImage's packager finds every library; it rewrites them for the AppImage.
# A default only: the .deb turns it off (packaging/debian/rules), and a
# plain set() would override that.
if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
