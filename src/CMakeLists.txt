# --- fs_core: the solver-neutral model, headless ----------------------------
# Geometry references, mesh, named sets, materials, steps, jobs. Qt Core and
# VTK's data model only: no widgets, so the command line and every test use
# it without a display. Solver back ends (CalculiX first) arrive as their own
# targets when their first function is written, not before (engineering
# principle 7: no empty targets).
add_library(fs_core STATIC
    core/ApplicationInfo.cpp
    core/Toolkit.cpp
)
target_include_directories(fs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fs_core PUBLIC Qt6::Core VTK::CommonCore VTK::CommonDataModel)

execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE FS_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT FS_BUILD)
    set(FS_BUILD "unknown")
endif()
target_compile_definitions(fs_core PRIVATE FS_VERSION="${PROJECT_VERSION}"
                                           FS_BUILD="${FS_BUILD}")

# Guard: the core never reaches for widgets or rendering.
get_target_property(fs_core_links fs_core LINK_LIBRARIES)
foreach(lib IN LISTS fs_core_links)
    if(lib MATCHES "^(Qt6::(Widgets|Gui|OpenGLWidgets)|VTK::(Rendering|GUISupport|Interaction))")
        message(FATAL_ERROR "fs_core must stay headless (found ${lib}). See CLAUDE.md.")
    endif()
endforeach()

# --- fs_gui: the window -------------------------------------------------------
add_library(fs_gui STATIC
    gui/MainWindow.cpp
)
target_link_libraries(fs_gui PUBLIC fs_core Qt6::Widgets Qt6::OpenGLWidgets
    VTK::RenderingCore VTK::RenderingOpenGL2 VTK::InteractionStyle VTK::GUISupportQt)
vtk_module_autoinit(TARGETS fs_gui MODULES ${VTK_LIBRARIES})

add_executable(FluidStress gui/main.cpp)
target_link_libraries(FluidStress PRIVATE fs_gui)

# --- fs-study: the same work without a window ----------------------------------
add_executable(fs-study cli/main.cpp)
target_link_libraries(fs-study PRIVATE fs_core)
