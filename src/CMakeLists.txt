# --- gf_core: the solver-neutral model, headless ----------------------------
# Geometry references, mesh, named sets, materials, steps, jobs. Qt Core and
# VTK's data model only: no widgets, so the command line and every test use
# it without a display. Solver back ends (CalculiX first) arrive as their own
# targets when their first function is written, not before (engineering
# principle 7: no empty targets).
add_library(gf_core STATIC
    core/ApplicationInfo.cpp
    core/Toolkit.cpp
)
target_include_directories(gf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gf_core PUBLIC Qt6::Core VTK::CommonCore VTK::CommonDataModel)

execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GF_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT GF_BUILD)
    set(GF_BUILD "unknown")
endif()
target_compile_definitions(gf_core PRIVATE GF_VERSION="${PROJECT_VERSION}"
                                           GF_BUILD="${GF_BUILD}")

# Guard: the core never reaches for widgets or rendering.
get_target_property(gf_core_links gf_core LINK_LIBRARIES)
foreach(lib IN LISTS gf_core_links)
    if(lib MATCHES "^(Qt6::(Widgets|Gui|OpenGLWidgets)|VTK::(Rendering|GUISupport|Interaction))")
        message(FATAL_ERROR "gf_core must stay headless (found ${lib}). See CLAUDE.md.")
    endif()
endforeach()

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
target_link_libraries(gf-study PRIVATE gf_core)
