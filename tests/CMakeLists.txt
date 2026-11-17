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

# The mesh part of a deck, written and read back.
add_executable(test_inp_writer test_inp_writer.cpp)
target_link_libraries(test_inp_writer PRIVATE gf_calculix)
add_test(NAME the_mesh_is_written_as_a_calculix_deck COMMAND test_inp_writer)

# GridForge's mesh through CalculiX and back: gf_mesh, the deck writer,
# ccx 2.23 (fixture box-gfmesh), the .frd reader; uniaxial tension known
# by hand at every node.
add_executable(test_mesh_to_ccx test_mesh_to_ccx.cpp)
target_link_libraries(test_mesh_to_ccx PRIVATE gf_calculix gf_mesh)
target_compile_definitions(test_mesh_to_ccx PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME gridforges_mesh_solves_in_ccx_as_by_hand COMMAND test_mesh_to_ccx)
# NEGATIVE CHECKS, 2026-11-17, deck writer and the round trip: the 9
# writer cases written first, all red against a stub (24 checks). Two
# round-trip expectations were wrong and corrected: the .frd keeps six
# digits of each coordinate, so its middle nodes are not halfway to 1e-9
# (now: each node where GridForge put it, to 5e-5); ccx's nodal stress is
# 209.998, extrapolated (now to 1e-4, as the other fixtures). Then each
# rule broken, the tests run, restored:
#   y and z swapped; ids by place, not by the grid's ids; C3D10 middle
#   nodes reversed; sixteen ids to a line; a space allowed in a set
#   name: each red;
#   -0 written as "-0": survived at first (no coordinate was -0.0: the
#   integer -0 is +0.0); the fixture's first z is now -0.0, and it is red.

# A running job's progress: the .sta and .cvg ccx appends to.
add_executable(test_job_progress test_job_progress.cpp)
target_link_libraries(test_job_progress PRIVATE gf_calculix)
target_compile_definitions(test_job_progress PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME job_progress_is_read_as_ccx_writes_it COMMAND test_job_progress)
# NEGATIVE CHECKS, 2026-11-16, job progress: the 6 cases written first,
# five red against a stub (the sixth, the two files agreeing, held with
# both empty). Then each rule undone, the test run, restored:
#   the U mark ignored or left on the attempt: five cases red;
#   an unfinished last line read as a row: the running-file case red;
#   force and displacement columns swapped: the residuals case red;
#   one .cvg header line too few: two cases red.

# Gmsh meshes: the .msh reader against files written by Gmsh 4.14.0, each
# a box with an answer known by hand (fixtures/gmsh/README.md).
add_executable(test_msh_reader test_msh_reader.cpp)
target_link_libraries(test_msh_reader PRIVATE gf_gmsh)
target_compile_definitions(test_msh_reader PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME msh_reader_reads_what_gmsh_wrote COMMAND test_msh_reader)

# The hand-off of a mesh to ParaView: one grid, physical groups as sets.
add_executable(test_mesh_handoff test_mesh_handoff.cpp)
target_link_libraries(test_mesh_handoff PRIVATE gf_gmsh)
target_compile_definitions(test_mesh_handoff PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME meshes_reach_paraview_with_their_groups COMMAND test_mesh_handoff)
# NEGATIVE CHECKS, 2026-11-17, .msh reader and mesh hand-off: the 18 cases
# written first, all red against stubs. Two test expectations were wrong
# and corrected against the files: the complete second-order file names
# its 9-node quadrangles (type 10) before its hexahedra; Gmsh writes ASCII
# coordinates to 16 digits, binary ones exactly. Then each rule broken,
# the tests run, restored:
#   tet10, hex20, prism15 middles in Gmsh's order; prism6 not mirrored;
#   tet4 mirrored; hex8 top and bottom swapped: the cell case red, each
#   (prism15 mirrored as prism6 is: volume -6000, VTK's quadratic wedge
#   faces the other way from its linear one);
#   groups of lower dimension as element sets too; groups looked up by
#   tag without their dimension; sets by descending tag; element sets
#   before node sets; unnamed groups as "Physical Surface 77": the set
#   cases red;
#   binary y and z swapped; binary sizes read as 4 bytes; node
#   coordinates read before the tags: the binary and reader cases red;
#   $Comments refused, not passed over: that case red;
#   the element's entity dimension unchecked: survived at first (no
#   fixture had it wrong); a block said to lie on a surface added, now red.
# Opened in ParaView 6.2.0 (Flatpak pvpython), 2026-11-17: all seven boxes,
# bounds 10 x 20 x 30, the assembly with Model, NodeSets (Solid, Fixed,
# Pulled, PhysicalSurface77, Edge, Origin), ElementSets (Solid).

# gf_mesh: STEP geometry meshed through Netgen, each fixture with an
# answer known without a mesher (fixtures/step/README.md).
add_executable(test_mesher test_mesher.cpp)
target_link_libraries(test_mesher PRIVATE gf_mesh)
target_compile_definitions(test_mesher PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME geometry_is_meshed_the_same_every_time COMMAND test_mesher)
# NEGATIVE CHECKS, 2026-11-17, gf_mesh: the 12 cases written first, all red
# against a stub (20 checks). Two expectations were wrong and corrected
# against the meshes: Netgen's size is a target, not a bound (the box's
# longest edge at 3 is 6.1; the case now asks that half the size give
# about eight times the cells); the dogbone's R30 transitions are concave,
# so straight edges gain volume (+3.2e-4), not lose it. Then each rule
# broken, the tests run, restored:
#   tet4 not turned: the box, size, dogbone and solids cases red;
#   tet10 middles in Netgen's order: both quadratic cases red;
#   solids numbered backwards: the solids case red;
#   order 2 not made: crashed (a 4-node cell read as 10); the quadratic
#     cases now stop when the cells are not quadratic, and are red;
#   faces numbered one off: survived at first (every face was still a
#     plane of the box); each face is now held to its plane in
#     OpenCASCADE's order, as Gmsh 4.14 lists it, and the case is red;
#   the recipe's size ignored: the size case red;
#   a size of 0 accepted: that case red.
# Found in ParaView, not by a test first: Netgen keeps points no element
# uses (the dogbone's four arc centres, y = -35 and 35). The case
# every_node_handed_on_belongs_to_an_element was written, seen red, then
# the points were left out.
# Opened in ParaView 6.2.0 (Flatpak, VTK's HDF reader), 2026-11-17: the
# dogbone (0 to 150, -10 to 10, 0 to 4; 6292 quadratic tetrahedra) and the
# two boxes, with the assembly Model, NodeSets (Solid1.., Face1..),
# ElementSets (Solid1..).
# NEGATIVE CHECKS, 2026-11-17, joining touching solids: the 3 cases
# written first, red (the two boxes shared only some nodes where they
# touch: 258 nodes at x = 10, at 217 places; overlapping boxes meshed
# without a word). Then each rule broken, the tests run, restored:
#   the solids not joined: both touching cases red;
#   overlap not checked: the overlap case red;
#   solids named backwards after joining: the solids case red;
#   shared faces not counted: the face-set case red (its provenance).
# The benchmark's two parts (tools/mesh-bench, block and cylinder, R12
# contact disk), order 2 at 2 mm: 27767 nodes at 27767 places; outside
# area 8659.90 against OpenCASCADE's 8661.95 (straight facets, -0.02 %);
# every mesher in the study left the contact face inside it (+10.3 %).

# The mesh quality report: tetrahedra with measures known by hand, then
# gf_mesh's meshes.
add_executable(test_mesh_quality test_mesh_quality.cpp)
target_link_libraries(test_mesh_quality PRIVATE gf_mesh)
target_compile_definitions(test_mesh_quality PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME mesh_quality_is_measured_as_by_hand COMMAND test_mesh_quality)
# NEGATIVE CHECKS, 2026-11-17, mesh quality: the 10 cases written first,
# all red against a stub (45 checks). Then each rule broken, the tests
# run, restored:
#   a flat tetrahedron not inverted; the orientation reversed; the mean
#   ratio's exponent 1/3; the dihedral angle taken between the normals;
#   the quadratic shape gradients wrong; the worst cells reversed; the
#   element id ignored: each red;
#   the centre's Jacobian not checked: survived at first (every folded
#   fixture folded at a node too); a tetrahedron folded only at its
#   centre (middle nodes 1.2 of the way to it: +0.28 at the nodes, -0.008
#   at the centre, solved for beforehand) is now red;
#   below 0.3 counted at 0.1: survived at first (the poor fixture was
#   below both); one at 0.19 added, now red.
# Against the benchmark's own referee (tools/mesh-bench/evaluate.py,
# numpy), on Netgen's dogbone at 2 mm: 6292 tetrahedra, mean ratio
# 0.61743259526 at worst and 0.88238374856 on average, smallest dihedral
# 26.989832 degrees, the same to 11 digits (and the same mesh as the
# released Netgen 6.2.2608 made in the study).

# The meshing study's benchmark, kept as a test: the five models at the
# study's sizes, quadratic, against OpenCASCADE's volumes and areas and
# the committed baseline (tools/mesh-bench/gridforge-baseline.json). The
# table is printed and written to mesher-benchmark.json here.
add_executable(test_mesher_benchmark test_mesher_benchmark.cpp)
target_link_libraries(test_mesher_benchmark PRIVATE gf_mesh)
target_compile_definitions(test_mesher_benchmark PRIVATE
    GF_BENCH="${CMAKE_SOURCE_DIR}/tools/mesh-bench")
add_test(NAME the_mesher_holds_the_benchmark COMMAND test_mesher_benchmark)
# NEGATIVE CHECKS, 2026-11-17, benchmark: the baseline raised by hand
# (dogbone's worst mean ratio +0.1, bracket's outside error a tenth): both
# red; the solids not joined: the two parts red (outside +10.3 %, as every
# mesher in the study); quadratic middle nodes misordered: every model
# red (folded, volume). First run (2026-11-17) against the study's
# Netgen 6.2.2608 numbers: the same cells for the four single solids
# (6292, 13304, 55862, 29140) and the same curved volume errors; the two
# parts, now joined, 7877 cells and outside -0.05 % (straight facets).

# gf-study mesh: a STEP in, one .vtkhdf out with its solids and faces as
# sets, said on the terminal.
add_test(NAME cli_meshes_a_step_with_its_solids_and_faces_as_sets
    COMMAND gf-study mesh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 3 --order 2
            -o ${CMAKE_CURRENT_BINARY_DIR}/mesh-out)
set_tests_properties(cli_meshes_a_step_with_its_solids_and_faces_as_sets PROPERTIES
    PASS_REGULAR_EXPRESSION "wrote [^\n]*mesh-out/box\\.vtkhdf \\(a mesh: [0-9]+ nodes, [0-9]+ elements, 8 sets from its solids and faces\\)\n  mesher: Netgen .*quality \\([^\n]*mesh-out/box\\.quality\\.json\\):\n  ready for a solver: no inverted or folded elements\n  [0-9]+ tetrahedra; mean ratio 0\\.[0-9]+ at worst")
add_test(NAME cli_mesh_without_a_size_is_refused
    COMMAND gf-study mesh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step)
set_tests_properties(cli_mesh_without_a_size_is_refused PROPERTIES WILL_FAIL TRUE)

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

# gf-study convert: a .frd in, one .vtkhdf out with the sets of the deck
# beside it (or, with --pvd, a .vtu series), said on the terminal.
add_test(NAME cli_converts_a_frd_to_vtkhdf_with_the_decks_sets
    COMMAND gf-study convert ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/bar-increments/bar.frd
            -o ${CMAKE_CURRENT_BINARY_DIR}/convert-out)
set_tests_properties(cli_converts_a_frd_to_vtkhdf_with_the_decks_sets PROPERTIES
    PASS_REGULAR_EXPRESSION "wrote [^\n]*convert-out/bar\\.vtkhdf \\(4 time points, 20 nodes, 4 elements, 6 sets from bar\\.inp\\)")
add_test(NAME cli_converts_a_frd_to_a_pvd_series
    COMMAND gf-study convert ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/bar-increments/bar.frd
            -o ${CMAKE_CURRENT_BINARY_DIR}/convert-out-pvd --pvd)
set_tests_properties(cli_converts_a_frd_to_a_pvd_series PROPERTIES
    PASS_REGULAR_EXPRESSION "wrote [^\n]*convert-out-pvd/bar\\.pvd \\(4 time points, 20 nodes, 4 elements, no sets: a \\.vtu series has no blocks\\)")
add_test(NAME cli_converts_a_gmsh_mesh_with_its_physical_groups
    COMMAND gf-study convert ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/gmsh/box-hex20/box.msh
            -o ${CMAKE_CURRENT_BINARY_DIR}/convert-out-msh)
set_tests_properties(cli_converts_a_gmsh_mesh_with_its_physical_groups PROPERTIES
    PASS_REGULAR_EXPRESSION "wrote [^\n]*convert-out-msh/box\\.vtkhdf \\(a mesh: 193 nodes, 24 elements, 7 sets from its physical groups\\)")
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

# NEGATIVE CHECKS, 2026-11-16, sets and VTKHDF: the 5 hand-off cases and
# the CLI test written first, red against stubs (no sets, "not written
# yet"). Then each rule undone, the test run, restored:
#   the writer always giving the first time point: the VTKHDF case red;
#   no provenance added by the writer: the provenance case red;
#   node sets as poly-vertices, not vertices: the VTKHDF case red;
#   a set's point data taken from the wrong points: the VTKHDF case red;
#   members missing from the results not said: that case red;
#   set ids taken as places (id - 1): survived at first (every fixture is
#     numbered 1 to n); the renumbered-bar case is now red.
# Opened in ParaView 6.2.0 (Flatpak pvpython, its VTK 9.7.1), 2026-11-16:
# bar-increments' four time points (DISP z 0.025 to 0.1), every array, the
# assembly with Model, NodeSets (NALL, FIXED, PULLED, X0, Y0), ElementSets
# (EALL). NOT seen there: the provenance. It is in the file (each block's
# FieldData, with per-step offsets; VTK 9.5 reads it back), but VTK 9.7.1's
# reader drops field data on every block of a collection, even from a file
# its own writer made (checked: a plain grid keeps it).

# NEGATIVE CHECKS, 2026-11-16, against cgx (fixtures */cgx-reference.txt,
# from cgx 2.21 by cgx-reference.py): written against the finished code,
# so first seen failing on the tolerance itself (uniaxial PS2 = 0 shown by
# cgx as 0.000001: single precision, now allowed as 4.8e-7 of the tensor's
# size). Then each rule broken, the test run, restored:
#   SYZ and SZX swapped before deriving: the cantilever red (2720 values);
#   von Mises 1e-4 relative off: every fixture red, at about 50 times the
#     tolerance;
#   PS2 and PS3 swapped: the cantilever red.

# NEGATIVE CHECKS, 2026-11-16, shells, plane elements and beams (fixtures
# shell-*, plane-cps4, beam-*, from strips.py): the reader and hand-off
# cases written first, red (types 8, 10, 12 unknown to the reader; tr3 and
# qu4 not mapped). The expanded-shell set case was green from the start (it
# records what the set code already did). Then each mapping broken, the
# test run, restored:
#   qu4 twisted, qu8 and tr6 middles turned: the strip case red; the
#     twisted qu4 only by the new corner-turn check (green without it);
#   be3 as end, middle, end: survived at first (a quadratic edge has no
#     edges, so no midpoint was checked); the edge's own midpoint check
#     added, and it is red.
# The strips also match cgx 2.21 at every node (cgx-reference.txt).

# The build stamp is taken at every build, not at configure time: files
# written by GridForge carry it as provenance.
add_test(NAME the_build_reported_is_the_source_built
    COMMAND ${CMAKE_COMMAND} -DCLI=$<TARGET_FILE:gf-study> -DSRC=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check-build-stamp.cmake)
# NEGATIVE CHECK, 2026-11-16, build stamp: written against the configure-
# time stamp, with an empty commit made and dropped again, building without
# reconfiguring: red ("reports build 53764798fb71-dirty, but the source
# is" the new commit). With the build-time stamp, the same: green.
