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
# NEGATIVE CHECKS, 2026-11-16, te10, increments and binary: the 8 new
# cases run against the ASCII-only reader: seven red (te10 refused, binary
# refused; the tip-displacement case red too, from a test bug: 0.025 * 3 is
# not 0.075 in floating point, now four literals). The eighth, the datasets
# of each increment, was green from the start: the reader already kept
# repeated datasets. Then each new rule undone, the test run, restored:
#   binary node y and z swapped: the binary mesh case red;
#   binary results one component off: the binary numbers case red;
#   a -3 expected after binary nodes: both binary cases red;
#   te10 taken as 4 nodes: the te10 case red;
#   a binary error placed by line, not by the record's byte: the
#     truncated-binary case red;
#   binary results read big-endian: the binary numbers case red.

# NEGATIVE CHECKS, 2026-11-16, he20, te4, pe6, pe15 (fixtures cube-*): the
# reader case and the hand-off case written first, red (type 4 refused by
# the reader; te4, pe6, pe15 refused by the converter). Then each mapping
# undone, the test run, restored:
#   he20 in the file's order: the hand-off case red (midside nodes off);
#   pe15 in the file's order: red the same way;
#   pe6 not mirrored: red (VTK's linear wedge faces the other way);
#   te4 mirrored: red.

# Named sets from the deck: the .frd has none.
add_executable(test_inp_sets test_inp_sets.cpp)
target_link_libraries(test_inp_sets PRIVATE gf_calculix)
target_compile_definitions(test_inp_sets PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME named_sets_are_read_from_the_deck COMMAND test_inp_sets)
# NEGATIVE CHECKS, 2026-11-16, named sets: the 8 cases written first, all
# red against a stub. Then each rule undone, the test run, restored:
#   an element's second line taken as a new element: the two-line case red;
#   names looked up by case: four cases red; set names inside a set by
#     case: three red; includes found from the working folder: six red;
#   GENERATE's step ignored, ids kept twice: both survived at first (step 1,
#     no overlap in the deck); the deck now has GENERATE 1, 3, 2 and Ends
#     lists node 1 again, and both are red.

# Derived stress quantities against hand answers.
add_executable(test_stress test_stress.cpp)
target_link_libraries(test_stress PRIVATE gf_core)
add_test(NAME derived_stresses_match_hand_answers COMMAND test_stress)

# The hand-off to ParaView: .frd to VTK grids, then .vtu files and a .pvd.
add_executable(test_results_handoff test_results_handoff.cpp)
target_link_libraries(test_results_handoff PRIVATE gf_calculix)
target_compile_definitions(test_results_handoff PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME results_reach_paraview_as_ccx_wrote_them COMMAND test_results_handoff)

# gf-study convert: a .frd in, a .pvd series out, said on the terminal.
add_test(NAME cli_converts_a_frd_to_a_pvd_series
    COMMAND gf-study convert ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/bar-increments/bar.frd
            -o ${CMAKE_CURRENT_BINARY_DIR}/convert-out)
set_tests_properties(cli_converts_a_frd_to_a_pvd_series PROPERTIES
    PASS_REGULAR_EXPRESSION "wrote [^\n]*convert-out/bar\\.pvd \\(4 time points, 20 nodes, 4 elements\\)")
# NEGATIVE CHECKS, 2026-11-16, stress and hand-off: all cases written
# first, red against stubs (stress 13 checks; hand-off 33; the CLI with no
# convert command). Then each rule undone, the suite run, restored:
#   he8 top and bottom swapped, te10 midside nodes 5 and 6 swapped: the
#     node-placement case red;
#   principal values with yz replaced by zx: the stress test red;
#   derived stress with yy and zz, or yz and zx, swapped: survived at
#     first (the bar is uniaxial, and permuting axes keeps its invariants);
#     now the cantilever's invariant case is red for both;
#   the writer not adding the provenance itself: survived at first (the
#     converter puts it on the grids); now the writer's own case is red;
#   the converter not putting it on the grids: the provenance case red;
#   a time point per dataset: the increments and CLI tests red;
#   the .pvd timestep written as the index: the writer case red;
#   no convert command: the CLI test red.
# Opened in ParaView 6.2.0 (Flatpak pvpython), 2026-11-16: bar-increments'
# four time points (tip uz 0.025 to 0.1), the cantilever's 480 quadratic
# tetrahedra, every array by name, the provenance.

# The build stamp is taken at every build, not at configure time: files
# written by GridForge carry it as provenance.
add_test(NAME the_build_reported_is_the_source_built
    COMMAND ${CMAKE_COMMAND} -DCLI=$<TARGET_FILE:gf-study> -DSRC=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check-build-stamp.cmake)
# NEGATIVE CHECK, 2026-11-16, build stamp: written against the configure-
# time stamp, with an empty commit made and dropped again, building without
# reconfiguring: red ("reports build 53764798fb71-dirty, but the source
# is" the new commit). With the build-time stamp, the same: green.
