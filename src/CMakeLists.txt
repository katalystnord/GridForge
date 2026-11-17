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
    core/Setup.cpp
    core/MaterialCard.cpp
    core/Job.cpp
    core/Study.cpp
    core/StudyOutline.cpp
    core/JobRunner.cpp
)
target_include_directories(gf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gf_core PUBLIC Qt6::Core VTK::CommonCore VTK::CommonDataModel
    VTK::CommonExecutionModel VTK::IOXML VTK::IOHDF PRIVATE yaml-cpp::yaml-cpp)

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
    calculix/InpSets.cpp
    calculix/InpWriter.cpp
    calculix/DeckWriter.cpp
    calculix/JobProgress.cpp
)
target_include_directories(gf_calculix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gf_calculix PUBLIC gf_core)

# --- gf_gmsh: Gmsh's meshes, headless ------------------------------------------
# Reads what Gmsh writes (.msh 4.1) and hands it to the core as a grid with
# its physical groups as sets. Gmsh is GPL-2.0-or-later and is run, never
# linked: nothing here comes from its source.
add_library(gf_gmsh STATIC
    gmsh/MshReader.cpp
    gmsh/MshToVtk.cpp
)
target_include_directories(gf_gmsh PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gf_gmsh PUBLIC gf_core)

# --- gf_mesh: GridForge's mesher, headless -------------------------------------
# Geometry in, a mesh out, the same every time. Netgen fills the volume
# (linked: LGPL-2.1, from our fork); what GridForge adds around it is here.
add_library(gf_mesh STATIC
    mesh/Mesher.cpp
    mesh/Quality.cpp
    mesh/Groups.cpp
)
target_include_directories(gf_mesh PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(gf_mesh SYSTEM PRIVATE ${NETGEN_OCC_INCLUDE_DIR})
target_link_libraries(gf_mesh PUBLIC gf_core PRIVATE nglib ngcore ${NETGEN_OCC_LIBRARIES})
target_compile_definitions(gf_mesh PRIVATE OCCGEOMETRY  # Netgen's OCC interface
    GF_NETGEN_VERSION="${NETGEN_VERSION}"
    GF_NETGEN_COMMIT="${GF_NETGEN_COMMIT}")

# --- gf_gui: the window -------------------------------------------------------
add_library(gf_gui STATIC
    gui/MainWindow.cpp
    gui/ResultsFile.cpp
    gui/NextStep.cpp
    gui/Style.cpp
    gui/Tone.cpp
    ${CMAKE_SOURCE_DIR}/resources/icons/icons.qrc
)
set_target_properties(gf_gui PROPERTIES AUTORCC ON)
target_link_libraries(gf_gui PUBLIC gf_core gf_mesh Qt6::Widgets Qt6::OpenGLWidgets Qt6::Concurrent
    VTK::FiltersGeometry VTK::RenderingAnnotation VTK::RenderingCore VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::GUISupportQt)
vtk_module_autoinit(TARGETS gf_gui MODULES ${VTK_LIBRARIES})
target_compile_definitions(gf_gui PRIVATE GF_SHIPPED_TOOLKITS="${CMAKE_SOURCE_DIR}/toolkits")
add_dependencies(gf_gui gf-calculix)

add_executable(GridForge gui/main.cpp)
target_link_libraries(GridForge PRIVATE gf_gui)

# --- gf-calculix: the CalculiX toolkit's adapter -------------------------------
# A separate program behind the toolkit door (toolkits spec): a job folder
# in, ccx run as its own process, progress as JSON lines, results out.
add_executable(gf-calculix calculix/adapter/main.cpp)
target_link_libraries(gf-calculix PRIVATE gf_calculix)

# --- gf-study: the same work without a window ----------------------------------
add_executable(gf-study cli/main.cpp)
target_link_libraries(gf-study PRIVATE gf_core gf_calculix gf_gmsh gf_mesh)
# The shipped toolkits' manifests, from the source tree until there is an
# install layout; their adapters (gf-calculix) sit beside gf-study.
target_compile_definitions(gf-study PRIVATE GF_SHIPPED_TOOLKITS="${CMAKE_SOURCE_DIR}/toolkits")
add_dependencies(gf-study gf-calculix)
