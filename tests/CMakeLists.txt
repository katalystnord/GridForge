# One executable per subject; walkthrough tests (later) carry their own
# private X server, so ctest is headless however it is started.
#
# NEGATIVE CHECKS, 2026-11-14, skeleton (each broken on purpose, seen red,
# restored):
#   - the application's name changed in ApplicationInfo.cpp: both version
#     tests red.
#   - a core header including <QWidget>: the build fails at gf_core itself
#     (no Widgets include path), before core_is_headless can run.
#   - Qt6::Widgets added to gf_core's link line: configure refuses ("gf_core
#     must stay headless").
#   Not yet covered: VTK rendering headers in the core are refused by the
#   configure guard only if linked; an include alone is not tested.

# The version every program reports is the project's, from one place.
add_test(NAME cli_reports_its_version COMMAND gf-study --version)
set_tests_properties(cli_reports_its_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^GridForge ${PROJECT_VERSION} \\(build ")
add_test(NAME gui_reports_its_version_without_a_display COMMAND GridForge --version)
set_tests_properties(gui_reports_its_version_without_a_display PROPERTIES
    PASS_REGULAR_EXPRESSION "^GridForge ${PROJECT_VERSION} \\(build "
    ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY=;QT_QPA_PLATFORM=")

# gf_core builds and links with no widget or rendering library: a source
# file that includes a Qt Widgets header fails to compile here.
add_executable(test_core_is_headless test_core_is_headless.cpp)
target_link_libraries(test_core_is_headless PRIVATE gf_core)
add_test(NAME core_is_headless COMMAND test_core_is_headless)

# Toolkits: the manifests, what each offers, broken ones refused.
add_executable(test_toolkits test_toolkits.cpp)
target_link_libraries(test_toolkits PRIVATE gf_core)
target_compile_definitions(test_toolkits PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    GF_SHIPPED_TOOLKITS="${CMAKE_SOURCE_DIR}/toolkits")
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

# CalculiX results: the .frd reader against files written by ccx 2.23, each
# with an answer known by hand (fixtures/calculix/*/README.md).
add_executable(test_frd_reader test_frd_reader.cpp)
target_link_libraries(test_frd_reader PRIVATE gf_calculix)
target_compile_definitions(test_frd_reader PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME frd_reader_reads_what_ccx_wrote COMMAND test_frd_reader)
# NEGATIVE CHECKS, 2026-11-16, frd reader: the 10 cases written first, all
# red against a stub (20 checks). Then each rule undone, the test run,
# restored:
#   data values read one column off: displacement and stress red (the
#     reaction alone stays green: a space before a positive number hides
#     the shift, so the negative -3.00000E-03 is what catches it);
#   a computed component (ALL, IEXIST 1) taken as supplied: five cases red;
#   unknown element types accepted: the unknown-type case red;
#   node y read into z: the mesh case red.
