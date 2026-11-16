# --- gf_core: the solver-neutral model, headless ----------------------------
# Geometry references, mesh, named sets, materials, steps, jobs. Qt Core and
# VTK's data model only: no widgets, so the command line and every test use
# it without a display. Solver back ends (CalculiX first) arrive as their own
# targets when their first function is written, not before (engineering
# principle 7: no empty targets).
add_library(gf_core STATIC
    core/ApplicationInfo.cpp
    core/Toolkit.cpp
    core/Stress.cpp
    core/ResultsSeries.cpp
)
target_include_directories(gf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gf_core PUBLIC Qt6::Core VTK::CommonCore VTK::CommonDataModel VTK::IOXML)

# The build stamp (git describe) is taken at every build, not at configure
# time: the files GridForge writes carry it as provenance.
set(GF_BUILD_STAMP ${CMAKE_CURRENT_BINARY_DIR}/BuildStamp.cpp)
add_custom_target(gf_build_stamp
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${GF_BUILD_STAMP}
            -P ${CMAKE_SOURCE_DIR}/tools/build-stamp.cmake
    BYPRODUCTS ${GF_BUILD_STAMP}
    COMMENT "Build stamp")
target_sources(gf_core PRIVATE ${GF_BUILD_STAMP})
add_dependencies(gf_core gf_build_stamp)
target_compile_definitions(gf_core PRIVATE GF_VERSION="${PROJECT_VERSION}")

# Guard: the core never reaches for widgets or rendering.
get_target_property(gf_core_links gf_core LINK_LIBRARIES)
foreach(lib IN LISTS gf_core_links)
    if(lib MATCHES "^(Qt6::(Widgets|Gui|OpenGLWidgets)|VTK::(Rendering|GUISupport|Interaction))")
        message(FATAL_ERROR "gf_core must stay headless (found ${lib}). See CLAUDE.md.")
    endif()
endforeach()

# --- gf_calculix: the CalculiX back end, headless ------------------------------
# Reads what ccx writes (.frd first) and hands it to the core as grids. ccx
# itself is GPL-2.0-only and is run, never linked: nothing here comes from
# its source.
add_library(gf_calculix STATIC
    calculix/FrdReader.cpp
    calculix/FrdToVtk.cpp
)
target_include_directories(gf_calculix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gf_calculix PUBLIC gf_core)

# --- gf_gui: the window -------------------------------------------------------
add_library(gf_gui STATIC
    gui/MainWindow.cpp
)
target_link_libraries(gf_gui PUBLIC gf_core Qt6::Widgets Qt6::OpenGLWidgets
    VTK::RenderingCore VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::GUISupportQt)
vtk_module_autoinit(TARGETS gf_gui MODULES ${VTK_LIBRARIES})

add_executable(GridForge gui/main.cpp)
target_link_libraries(GridForge PRIVATE gf_gui)

# --- gf-study: the same work without a window ----------------------------------
add_executable(gf-study cli/main.cpp)
target_link_libraries(gf-study PRIVATE gf_core gf_calculix)
