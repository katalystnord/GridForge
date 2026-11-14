# One executable per subject; walkthrough tests (later) carry their own
# private X server, so ctest is headless however it is started.
#
# NEGATIVE CHECKS, 2026-11-14, skeleton (each broken on purpose, seen red,
# restored):
#   - the application's name changed in ApplicationInfo.cpp: both version
#     tests red.
#   - a core header including <QWidget>: the build fails at fs_core itself
#     (no Widgets include path), before core_is_headless can run.
#   - Qt6::Widgets added to fs_core's link line: configure refuses ("fs_core
#     must stay headless").
#   Not yet covered: VTK rendering headers in the core are refused by the
#   configure guard only if linked; an include alone is not tested.

# The version every program reports is the project's, from one place.
add_test(NAME cli_reports_its_version COMMAND fs-study --version)
set_tests_properties(cli_reports_its_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^FluidStress ${PROJECT_VERSION} \\(build ")
add_test(NAME gui_reports_its_version_without_a_display COMMAND FluidStress --version)
set_tests_properties(gui_reports_its_version_without_a_display PROPERTIES
    PASS_REGULAR_EXPRESSION "^FluidStress ${PROJECT_VERSION} \\(build "
    ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY=;QT_QPA_PLATFORM=")

# fs_core builds and links with no widget or rendering library: a source
# file that includes a Qt Widgets header fails to compile here.
add_executable(test_core_is_headless test_core_is_headless.cpp)
target_link_libraries(test_core_is_headless PRIVATE fs_core)
add_test(NAME core_is_headless COMMAND test_core_is_headless)

# Toolkits: the manifests, what each offers, broken ones refused.
add_executable(test_toolkits test_toolkits.cpp)
target_link_libraries(test_toolkits PRIVATE fs_core)
target_compile_definitions(test_toolkits PRIVATE
    FS_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    FS_SHIPPED_TOOLKITS="${CMAKE_SOURCE_DIR}/toolkits")
add_test(NAME toolkits_are_read_offered_and_refused COMMAND test_toolkits)
# NEGATIVE CHECKS, 2026-11-14, toolkits: the 8 cases written first, all red
# against stubs (17 checks). Then each rule undone, the test run, restored:
#   features not compared: a_missing_feature_is_named red;
#   kinds not compared: the run crashes (null stage dereferenced, exit 139);
#   a second toolkit of one id appended, not replacing: the override case red;
#   no check for {job}, for the trust word or for the stage word: the
#     broken-manifest case red, each;
#   requirements ignored: the missing-programs case red;
#   the toolkit's own folder not searched for its program: the
#     missing-programs and command cases red.
