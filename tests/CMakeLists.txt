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

# A study's set-up read from study.json and checked (study spec, section 4).
add_executable(test_setup test_setup.cpp)
target_link_libraries(test_setup PRIVATE gf_core)
add_test(NAME the_setup_is_read_and_checked COMMAND test_setup)
# NEGATIVE CHECK, 2026-11-18, a load's direction: the case red against a
# stub; then an axis direction's sign, and its other components, not
# checked: each red.
# NEGATIVE CHECKS, 2026-11-17, set-up: the 5 cases written first, all red
# against a stub (32 checks). Then each rule broken, the test run,
# restored: a solid given two materials allowed; units defaulted when
# missing; any direction name taken; a group of faces given a material;
# y read as z; face 0 accepted: each red.

# The window's support and load form, and the study.json entry it stands
# for (study spec 4.6; docs/survey/loads-and-supports.md).
add_executable(test_load_choice test_load_choice.cpp)
target_link_libraries(test_load_choice PRIVATE gf_core)
add_test(NAME a_support_or_load_is_chosen_as_in_the_precedents COMMAND test_load_choice)
# NEGATIVE CHECKS, 2026-11-18, the load form: the 6 cases written first,
# all red against stubs. Then each rule broken, the test run, restored: a
# measured normal taken as on an axis only when exactly so; components of
# nothing written; others held not read back; every direction held not
# written as fixed: each red.

# FreeCAD material cards in a study's units (fixtures/materials).
add_executable(test_material_card test_material_card.cpp)
target_link_libraries(test_material_card PRIVATE gf_core)
target_compile_definitions(test_material_card PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME material_cards_are_read_in_the_studys_units COMMAND test_material_card)
# NEGATIVE CHECKS, 2026-11-17, material cards: the 8 cases written first,
# red against a stub (the fill case crashed on the missing values). Then
# each rule broken, the tests run, restored: density in mm off by 1000;
# GPa taken as kPa; an unknown unit passed as is; the card overruling the
# study's own value; the LinearElastic model (TenSense's) not read: each
# red; a Poisson's ratio with a unit passed: survived at first (no card
# had one); poisson-unit.FCMat added, now red.

# A study read from study.json and checked (study spec, sections 1, 2).
add_executable(test_study test_study.cpp)
target_link_libraries(test_study PRIVATE gf_core)
target_compile_definitions(test_study PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME a_study_is_read_and_checked COMMAND test_study)
# NEGATIVE CHECK, 2026-11-17, addFaceGroup: both cases red against a stub
# (6 checks).
# NEGATIVE CHECKS, 2026-11-18, items from the form and directions: the 3
# cases written first, red against stubs. Then each rule broken, the test
# run, restored: a normal not turned round with its numbers; an axis kept
# without its component (green at first: a case added, now red); a step's
# fixed support put among its loads: each red.
# NEGATIVE CHECKS, 2026-11-17, study and outline: the cases written first,
# red against stubs. Then: a link's output not checked; a plastic step not
# shown with large displacements: each red.

# A solve stage's job folder: written from a set-up and mesh, read back.
add_executable(test_test_curve test_test_curve.cpp)
target_link_libraries(test_test_curve PRIVATE gf_calculix)
target_compile_definitions(test_test_curve PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME a_measured_curve_is_set_beside_the_predicted_one COMMAND test_test_curve)
# NEGATIVE CHECKS, 2026-11-18, measured against predicted: the cases
# written first, red against a stub (10). Then each rule broken, the test
# run, restored: the gauge's points not moved with the part: red (2); the
# force taken across the gauge, not along it: red; the first crossing's
# guard dropped: survived at first (the unloading case could not tell),
# the case sharpened (and a curve starting above the strain added): red.
# The window's case (a_test_s_curve_is_set_beside_the_run_s) written
# first (it did not build: no CurvePlot); then Compare enabled before a
# solve: red. The plot's ticks: 0.6 made by adding 0.2 three times was
# red against the exact list, the ticks now made as whole multiples.

add_executable(test_convergence test_convergence.cpp)
target_link_libraries(test_convergence PRIVATE gf_core)
add_test(NAME a_mesh_convergence_check_says_how_far_a_result_can_be_trusted COMMAND test_convergence)
# NEGATIVE CHECKS, 2026-11-18, the convergence check: the cases written
# first, red against a stub (14, then the peaks' 3). Then each rule broken,
# the test run, restored: the band's 1.25 as Roache's two-mesh 3.0: red
# (2); oscillation not told apart: red (2); the extrapolation's sign
# turned: red (4). The finer recipe (test_mesher) red before it was read
# or applied. The formulas checked against the aorta GCI paper (arXiv
# 2309.05509, its equations 2 to 10) and NASA NPARC's spatial convergence
# tutorial; the order for unequal ratios is solved from f = f0 + C h^p
# itself (Celik's iteration not checked against its paper). NOT held by
# the CLI test: that the three meshes differ (the stand-in ccx replays one
# result); the mesher's test holds finer, the README's run real ccx.
# NEGATIVE CHECKS, 2026-11-18, a finer mesh on a group ("refine"): the
# mesher's case red before faceSizes was applied (36 nodes on the face
# against 38); meshParameters' case red against a stub (5); setFinerMesh's
# red against a stub. Then the window handed the worker the raw
# parameters: the walkthrough red (508 nodes, then 508). The CLI grep red
# when asked for "at 9". NOT held: that a refined group's faces changing
# remeshes a kept mesh (the keep-mesh rule compares resolved parameters).

add_executable(test_mesh_run test_mesh_run.cpp)
target_link_libraries(test_mesh_run PRIVATE gf_core)
target_compile_definitions(test_mesh_run PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    GF_STUDY="$<TARGET_FILE:gf-study>")
add_dependencies(test_mesh_run gf-study)
add_test(NAME meshing_runs_in_its_own_process_and_cannot_take_the_window_down COMMAND test_mesh_run)
# NEGATIVE CHECKS, 2026-11-18, meshing in its own process: the 9 cases
# written first, red against a stub (the file cases failed at once, the
# process cases waited out their time). The window's two cases (Stop, a
# crashing stand-in) red against the window meshing on a thread: Stop
# not enabled, the crash not said. The open box (test_mesher) ended the
# test program (Netgen: double free) before the check; the ball ended
# gf-study (our edge code asking a pole for points) before its fix. Then
# each rule broken, the tests run, restored: seams counted as openings:
# the benchmark red (bracket, twoparts, duct refused); poles counted: the
# ball red.

add_executable(test_job test_job.cpp)
target_link_libraries(test_job PRIVATE gf_mesh)
target_compile_definitions(test_job PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME a_job_folder_holds_everything_a_toolkit_needs COMMAND test_job)
# NEGATIVE CHECKS, 2026-11-17, job folder: the 4 cases written first, all
# red against a stub. Then each rule broken, the tests run, restored: set
# members lost; the values' sources dropped (crashed: read back missing);
# the plastic block left outside the job: each red; the set arrays left
# on the grid: survived at first, now checked, red.

# gf-calculix with a stand-in ccx replaying ccx 2.23's own files.
add_executable(test_calculix_adapter test_calculix_adapter.cpp)
target_link_libraries(test_calculix_adapter PRIVATE gf_mesh)
target_compile_definitions(test_calculix_adapter PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    GF_ADAPTER="$<TARGET_FILE:gf-calculix>"
    GF_STUDY="$<TARGET_FILE:gf-study>")
add_dependencies(test_calculix_adapter gf-calculix gf-study)
add_test(NAME the_calculix_adapter_runs_a_job COMMAND test_calculix_adapter)
# NEGATIVE CHECKS, 2026-11-17, the adapter: the 6 cases written first, all
# red against a stub. Then each rule broken, the tests run, restored:
#   sets left out of the results: red;
#   the progress from the step's time only: survived at first (one step:
#   the same); box-twosteps, solved by ccx, added, now red;
#   an *ERROR with exit 0 passed: survived twice (the stand-in's quiet
#   failure first wrote no results either); it now writes them, now red;
#   the plastic block's path not seen from work/: no build at first (an
#   empty if); kept, a plastic job added, now red.
# NEGATIVE CHECKS, 2026-11-17, increments and box-plastic: the set-up and
# deck cases written first, red. box-plastic's checks first failed on the
# test's own mistakes (one time point assumed; the pull's tolerance not
# counting the coordinates' rounding; the card not filled before the
# deck was compared), corrected. Then: the plastic block left out of the
# deck, no room for the increments (INC): each red. The hand answer's
# last 0.11 % against ccx's converged value is not explained yet (its
# README).
# NEGATIVE CHECKS, 2026-11-17, cancel: both cases written first, red (the
# adapter and gf-study killed outright). Then each rule broken: the
# adapter ignoring SIGTERM: red; gf-study not passing the cancel on:
# survived at first (its 10 s SIGKILL still stopped everything in time);
# the toolkit's own "cancelled: ... after step 1, increment 1" within 9 s
# is now required, red. By hand with FreeCAD's Flatpak ccx: ccx ran on to
# the end after a cancel until tools/ccx-flatpak used --die-with-parent;
# since, it stops (job.sta frozen at the increment said).
# NEGATIVE CHECKS, 2026-11-17, single precision: the writer case changed
# first (float arrays, values to single precision, the provenance line),
# red against the double writer; then the conversion skipped: red.
# NEGATIVE CHECKS, 2026-11-17, extrapolation notes: the case written first,
# red against a stub. Then each rule broken, the test run, restored: PE
# counted below -1 instead of 0: red; the least taken as the last seen:
# survived at first (the least came last); the fixture's least now
# first, red.
# Found by the real run (the dogbone, examples/dogbone-plastic), not by
# the stand-in: ccx 2.23 refuses a number with an exponent longer than 21
# characters (a Netgen coordinate, -4.7717021973877325e-06); the deck
# writer now keeps every number to 20 (test_inp_writer).

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

# A CalculiX job's deck from a study's set-up, around GridForge's mesh.
add_executable(test_deck_writer test_deck_writer.cpp)
target_link_libraries(test_deck_writer PRIVATE gf_calculix gf_mesh)
target_compile_definitions(test_deck_writer PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME the_setup_is_written_as_a_calculix_deck COMMAND test_deck_writer)
# NEGATIVE CHECKS, 2026-11-17, the deck from a set-up: the 13 cases
# written first, all red against stubs (28 checks). Then each rule
# broken, the tests run, restored: symmetry on the wrong axis; NLGEOM not
# turned on by a plastic material; raw lines dropped; a solid without a
# material passed; only the first face of a group taken; stress written
# as strain; a section's group name not made ccx-safe: each red (and the
# box-setup round trip red where the deck changes).
# NEGATIVE CHECKS, 2026-11-17, forces, pressure and gravity: the load
# cases written first, red (the set-up refused them; the deck had none).
# Then each rule broken, the tests run, restored: faces S2 and S3 swapped;
# a force on the corners of quadratic faces; a force not divided by the
# area; gravity reversed: each red (the deck cases, and the round trips
# where the deck changes); inner faces taken as outside: survived at
# first (no group of the box lies inside); a load on the two boxes'
# contact face is now refused, and that case is red.
# box-gravity first failed the round trip (reaction 0.4521 against
# 0.4650): ccx's FORC is the reaction less the node's own load; the
# test now adds the top nodes' consistent gravity load back (README). The mesh writer's
# name rule (spec 4.2: underscores, said in a comment; two names written
# alike refused) replaced its refusal of spaces: both cases seen red
# against the old rule first.

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
# The mesh's outer surface, each triangle on its geometry face: the box's
# six faces have areas known without a mesher (fixtures/step/README.md).
add_executable(test_outer_surface test_outer_surface.cpp)
target_link_libraries(test_outer_surface PRIVATE gf_mesh)
target_compile_definitions(test_outer_surface PRIVATE
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME the_outer_surface_knows_its_geometry_faces COMMAND test_outer_surface)
# NEGATIVE CHECK, 2026-11-17: red against a stub returning an empty surface
# (8 checks). The flat-face check at first compared coordinates exactly;
# loosened to 1e-9 mm before the first run, not after a failure.
# NEGATIVE CHECKS, 2026-11-18, normals and centres: the 2 cases red against
# stubs. Then curved faces not refused; faces facing apart not refused;
# the centre not weighted by area: each red. The dogbone's 4 curved and 10
# flat faces are counted from its STEP file (CYLINDRICAL_SURFACE, PLANE).
# NEGATIVE CHECKS, 2026-11-18, the geometry's edges: the 2 cases red
# against stubs. Then quadratic segments not split at their middle node
# (green at first: straight edges cannot tell; a count added, now red);
# curved edges given a direction: each red. Every segment Netgen lists
# kept, duplicates and all: green, as Netgen's list holds each once; the
# set stays, and its comment says so instead of a guess.

# What the window draws for a step's supports and loads.
add_executable(test_markers test_markers.cpp)
target_link_libraries(test_markers PRIVATE gf_gui)
target_compile_definitions(test_markers PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME supports_and_loads_are_drawn_where_and_how_they_act COMMAND test_markers)
# NEGATIVE CHECKS, 2026-11-18, markers: the 2 cases red against a stub.
# Then each rule broken, the test run, restored: cones not into the solid;
# arrows always out of the faces; a pressure drawn outward; a fixed support
# drawn with one cone: each red.
# NEGATIVE CHECK, 2026-11-18, the nearest edge: red against a stub; then a
# segment's ends ignored (measured to its line): red.

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
# NEGATIVE CHECKS, 2026-11-18, thin walls: before measureWalls, the plate
# case red (no "face thickness"); with face sizes alone, red at 2514 of
# 2568 tetrahedra spanning the plate; with points at half the thickness,
# red at 2653 of 2689 (Netgen spans 2h); at a third, 420 of 12003. Without
# the width rule, the 10 mm box at 5 was made finer: red. recipeFrom, the
# field and its check, and --through-wall each red before their code.
# The benchmark baseline written again for it: thinwall 55862 to 665268
# cells (worst mean ratio 0.276 to 0.404), dogbone 6292 to 24184, bracket
# 13304 to 33283, duct 29140 to 34839; twoparts the same.
# 2026-11-18, the maths: with glibc's FMA routines hidden
# (GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA,-FMA4), four of the five
# benchmark meshes moved here (the cells the same, the nodes not); the
# flat-walled one did not. The benchmark now runs itself again with those
# plain routines on every machine, and its hashes are written under them.
# NEGATIVE CHECK, 2026-11-18, the mesh hash: one character of the
# dogbone's committed hash changed, red ("the mesh is not the committed
# one"); two runs here gave the same five hashes.

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

# gf-study mesh --through-wall: the thin wall's faces made finer by
# default, said; 0 leaves them to Netgen alone; a number it cannot be,
# refused.
add_test(NAME cli_makes_a_thin_wall_finer_and_says_so
    COMMAND gf-study mesh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/plate.step --size 5
            -o ${CMAKE_CURRENT_BINARY_DIR}/thin-out)
set_tests_properties(cli_makes_a_thin_wall_finer_and_says_so PROPERTIES
    PASS_REGULAR_EXPRESSION "recipe: largest element 5, order 1, 2 elements through a wall\n[^\n]*\n[^\n]*\n  thin walls: 2 faces 1\.5 thick, sized 0\.5 for 2 elements through")
add_test(NAME cli_through_wall_0_leaves_a_thin_wall_alone
    COMMAND gf-study mesh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/plate.step --size 5 --through-wall 0
            -o ${CMAKE_CURRENT_BINARY_DIR}/thin-off)
set_tests_properties(cli_through_wall_0_leaves_a_thin_wall_alone PROPERTIES
    PASS_REGULAR_EXPRESSION "recipe: largest element 5, order 1, no elements added through thin walls"
    FAIL_REGULAR_EXPRESSION "thin walls:")
# GF_MESH_TRACE: a fingerprint after each of the mesher's stages, to find
# where two machines' meshes of the same study part.
# (Without GF_MESH_TRACE no trace line is written: the test red.)
add_test(NAME cli_traces_the_meshers_stages
    COMMAND gf-study mesh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 5
            -o ${CMAKE_CURRENT_BINARY_DIR}/trace-out)
set_tests_properties(cli_traces_the_meshers_stages PROPERTIES ENVIRONMENT "GF_MESH_TRACE=1"
    PASS_REGULAR_EXPRESSION "trace: walls 6 faces, 0 points, [0-9a-f]+\ntrace: edges [^\n]*\ntrace: surface [^\n]*\ntrace: volume [^\n]*\ntrace: optimised [^\n]*[0-9a-f]+\n")
add_test(NAME cli_through_wall_not_a_whole_number_is_refused
    COMMAND gf-study mesh ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 5 --through-wall 1.5)
set_tests_properties(cli_through_wall_not_a_whole_number_is_refused PROPERTIES WILL_FAIL TRUE)

# gf-study solve: the job written, the CalculiX toolkit found by its
# manifest and run (with the stand-in ccx), its progress shown.
# gf-study converge: three meshes, each finer by the ratio, three solves
# and the verdict. The stand-in ccx replays the same results whatever the
# mesh, so the values cannot move: "the same on all three meshes". What
# real ccx gives is in examples/dogbone-bend/README.md.
# gf-study solve says progress and the toolkit's own words; the solver's
# residual at each iteration only with --verbose (box-plastic replayed:
# its 50 increments' residuals in its job.cvg) (CRM 661's 52 increments
# gave 104 such lines, 2026-11-19). The adapter's info lines stay.
add_test(NAME cli_solve_keeps_the_residuals_for_verbose
    COMMAND gf-study solve ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-plastic/tension.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 5 --order 2
            -o ${CMAKE_CURRENT_BINARY_DIR}/solve-quiet)
set_tests_properties(cli_solve_keeps_the_residuals_for_verbose PROPERTIES
    ENVIRONMENT "GF_CCX=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fake-ccx/ccx;FAKE_CCX_FROM=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-plastic"
    PASS_REGULAR_EXPRESSION "100%  step 1, increment 50\n.*info: ccx finished; reading its results"
    FAIL_REGULAR_EXPRESSION "force residual")
add_test(NAME cli_solve_says_the_residuals_when_verbose
    COMMAND gf-study solve ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-plastic/tension.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 5 --order 2 --verbose
            -o ${CMAKE_CURRENT_BINARY_DIR}/solve-verbose)
set_tests_properties(cli_solve_says_the_residuals_when_verbose PROPERTIES
    ENVIRONMENT "GF_CCX=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fake-ccx/ccx;FAKE_CCX_FROM=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-plastic"
    PASS_REGULAR_EXPRESSION "force residual: ")
# NEGATIVE CHECKS, 2026-11-19, a quiet solve: both written first against
# box-setup's replay, whose one residual line... was none: the quiet case
# passed for nothing, so both moved to box-plastic's 50 increments; then
# both red (the residuals printed always; --verbose unknown). The
# adapter's case (the_adapter_says_what_it_does_after_ccx) red before
# its two lines.

# gf-study --refine: a group of faces meshed finer, said in the mesh's
# provenance; a group not there refused.
add_test(NAME cli_meshes_a_group_finer_when_asked
    COMMAND sh -c "\"$1\" deck \"$2/calculix/box-setup/tension.json\" \"$2/step/box.step\" --size 5 --refine Pulled=1 -o \"$3\" && grep -q '^\\*\\* finer faces: face 2 at 1$' \"$3/mesh.inp\""
            refine $<TARGET_FILE:gf-study> ${CMAKE_CURRENT_SOURCE_DIR}/fixtures ${CMAKE_CURRENT_BINARY_DIR}/refine-out)
add_test(NAME cli_refuses_to_refine_a_group_not_there
    COMMAND gf-study deck ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-setup/tension.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 5 --refine Nope=1 -o ${CMAKE_CURRENT_BINARY_DIR}/refine-no)
set_tests_properties(cli_refuses_to_refine_a_group_not_there PROPERTIES
    PASS_REGULAR_EXPRESSION "gf-study: the mesh's refine: no group \"Nope\"")

# gf-study compare: the solve's results (cli_solves_a_setup_through_its_
# toolkit's) beside a TenSense curve. The words are what is checked here;
# the arithmetic is test_test_curve's. A gauge not given is refused.
add_test(NAME cli_sets_a_measured_curve_beside_the_predicted
    COMMAND gf-study compare ${CMAKE_CURRENT_BINARY_DIR}/solve-out/results/results.vtkhdf
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tensense/Zx3.vtp --force-on Pulled --gauge "2,10,15;8,10,15"
            --times 4 -o ${CMAKE_CURRENT_BINARY_DIR}/compare-out.csv)
set_tests_properties(cli_sets_a_measured_curve_beside_the_predicted PROPERTIES
    DEPENDS cli_solves_a_setup_through_its_toolkit
    PASS_REGULAR_EXPRESSION "measured: Zx3\\.vtp \\(TenSense's analysed curve\\)\npredicted: results\\.vtkhdf, the force on Pulled, an extensometer of 6 from \\(2, 10, 15\\), its force times 4 \\(the model is 1/4 of the test piece\\)\npredicted against measured, at 1 strain [^\n]*\nwrote [^\n]*compare-out\\.csv")
add_test(NAME cli_refuses_a_comparison_without_its_gauge
    COMMAND gf-study compare ${CMAKE_CURRENT_BINARY_DIR}/solve-out/results/results.vtkhdf
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/tensense/Zx3.vtp --force-on Pulled)
set_tests_properties(cli_refuses_a_comparison_without_its_gauge PROPERTIES
    PASS_REGULAR_EXPRESSION "gf-study: compare needs --force-on GROUP and --gauge")

add_test(NAME cli_checks_the_results_against_the_mesh
    COMMAND gf-study converge ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-setup/tension.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 10 --ratio 1.5
            -o ${CMAKE_CURRENT_BINARY_DIR}/converge-out)
set_tests_properties(cli_checks_the_results_against_the_mesh PROPERTIES
    ENVIRONMENT "GF_CCX=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fake-ccx/ccx;FAKE_CCX_FROM=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-setup"
    PASS_REGULAR_EXPRESSION "mesh 1 of 3: every size over 1\n.*mesh 2 of 3: every size over 1\\.5\n.*mesh 3 of 3: every size over 2\\.25\n.*mesh convergence \\(ratio 1\\.5; Celik et al\\. 2008, grid convergence index\\):\n.*  largest displacement: [0-9.e-]+ mm on the finest mesh; the same on all three meshes\n.*wrote [^\n]*converge-out/convergence\\.json")
add_test(NAME cli_refuses_a_convergence_ratio_below_1_3
    COMMAND gf-study converge ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-setup/tension.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 10 --ratio 1.1 -o ${CMAKE_CURRENT_BINARY_DIR}/converge-no)
set_tests_properties(cli_refuses_a_convergence_ratio_below_1_3 PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_solves_a_setup_through_its_toolkit
    COMMAND gf-study solve ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-setup/tension.json
            ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/step/box.step --size 5 --order 2
            -o ${CMAKE_CURRENT_BINARY_DIR}/solve-out)
set_tests_properties(cli_solves_a_setup_through_its_toolkit PROPERTIES
    ENVIRONMENT "GF_CCX=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/fake-ccx/ccx;FAKE_CCX_FROM=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/calculix/box-setup"
    PASS_REGULAR_EXPRESSION "solving with CalculiX .*100%  step 1, increment 1\n.*results: [^\n]*solve-out/results/results\\.vtkhdf \\(solved by CalculiX Version 2\\.23\\)")

# The banner's next step for each state of the window.
add_executable(test_next_step test_next_step.cpp)
target_link_libraries(test_next_step PRIVATE gf_gui)
add_test(NAME the_banner_says_the_one_next_step COMMAND test_next_step)

# A run's results as the window reads them (no display needed).
add_executable(test_results_file test_results_file.cpp)
target_link_libraries(test_results_file PRIVATE gf_gui gf_calculix)
target_compile_definitions(test_results_file PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME a_runs_results_are_read_for_the_window COMMAND test_results_file)

# --- Walkthrough: the real window (layout A) --------------------------------
add_executable(test_main_window_walkthrough test_main_window_walkthrough.cpp)
target_link_libraries(test_main_window_walkthrough PRIVATE gf_gui Qt6::Test)
target_compile_definitions(test_main_window_walkthrough PRIVATE GF_EXAMPLES="${CMAKE_SOURCE_DIR}/examples"
    GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    GF_ADAPTER_DIR="$<TARGET_FILE_DIR:gf-calculix>")
add_dependencies(test_main_window_walkthrough gf-calculix)
vtk_module_autoinit(TARGETS test_main_window_walkthrough MODULES ${VTK_LIBRARIES})
# HEADLESS BY CONSTRUCTION (TenSense's and SurView's lesson): the test shows
# real windows, so it runs on its own private X server (xvfb-run, 24-bit
# for the GL context, software GL so no graphics card is involved); started
# any other way, it puts itself there (ensureHeadless in the test).
find_program(GF_XVFB_RUN xvfb-run)
if(GF_XVFB_RUN)
    add_test(NAME the_window_is_walked_through
             COMMAND ${GF_XVFB_RUN} -a -s "-screen 0 1700x1100x24" $<TARGET_FILE:test_main_window_walkthrough>)
else()
    message(WARNING "xvfb-run not found: the walkthrough would open windows on the desktop. Install xvfb.")
    add_test(NAME the_window_is_walked_through COMMAND test_main_window_walkthrough)
endif()
# NEGATIVE CHECKS, 2026-11-17, the window: its first run crashed, which is
# what it is for: (1) a window closed while meshing left the meshing
# thread writing into it (now the mesh comes back through the future, and
# a window waits for its meshing before it goes); (2) two meshings at once
# crashed inside Netgen (global state; gf_mesh now meshes one at a time,
# and test_mesher meshes on two threads at once: it crashed 1 run in 3
# before the lock, none in 6 after).
# NEGATIVE CHECK, 2026-11-17, a run without results.json: red before
# (the tree said "run 1,  by ", blank, nothing said).
# NEGATIVE CHECK, 2026-11-17, Properties scrolled: red before the scroll area.
# NEGATIVE CHECKS, 2026-11-17, curved faces drawn curved: the volume case
# red with corner triangles only (8706.38 against 8699.69 both ways), the
# edges case red against a stub. The bound was first a quarter of the
# corner triangles' error; the split met it at 0.2501 (half the spacing
# quarters a chord's sag), so it asks below half. Drawing it found that
# per-triangle colours (cell scalars) are not drawn at all on Mesa's
# llvmpipe once a mesh's own edges are off: the four ember cases red; drawn
# with three points per triangle and point colours: green.
# NEGATIVE CHECKS, 2026-11-17, renaming and deleting groups: the cases red
# against stubs (13 checks) and before Properties offered it; then the
# supports and loads not renamed: red; the material assignment not renamed
# (found by reading study.json, not by a failure: "assign" names groups):
# red; uses not checked before deleting: red; the assignment not checked:
# red; the renamed group not selected again: red.
# NEGATIVE CHECKS, 2026-11-17, the face under the mouse: the case red before
# hover was written; then no tint: red; not named: red; the name left after
# the tint: red. Two at first green: a tint while dragging (VTK turns the
# camera on the same move, so the pixels cannot tell; the status bar does:
# red), and the tint kept on leaving the view (the test only moved off the
# model; a real Leave event then: red).
# NEGATIVE CHECKS, 2026-11-17, the Steps dock: the cases red against a stub
# table and before the dock was a table; then a value never said kept:
# red; nothing kept from step to step: red; a click selecting nothing: red.
# NEGATIVE CHECKS, 2026-11-17, the deformed shape: the cases red against
# stubs (no deformation, a factor of 1 always); then the factor ignored:
# red; the shape not said beside the increment: red; the rounding margin
# taken out: at first green (a feared case, 10 / 1e-5, rounds right
# anyway), so a case found where it matters (5 / 1e-5: 20000, not 50000):
# red.
# NEGATIVE CHECKS, 2026-11-17, the Deck dock: the protocol, adapter and
# walkthrough cases red before the role was read, said or shown (6 cases);
# the runner taking a deck line for the results: red (the stand-in toolkit
# says its deck after its results).
# NEGATIVE CHECKS, 2026-11-17, picking faces: the cases red against no
# picking at all (Picked empty, nothing lit); then each rule taken out, red:
# a drag counted as a click; the groups back among the mesh's inputs (the
# box meshed twice); the groups' sets not made again on the kept mesh (at
# first green: the check had landed in another case; moved, then red);
# Ctrl+click not taking a picked face out; a selected group not lit. The
# ember check reads a 9 x 9 patch by proportion: one pixel read 159 119 106
# on a mesh edge, and the dimly lit side 106 43 14.
# NEGATIVE CHECKS, 2026-11-17, Properties after a run and the legend: the
# study's "Runs" not refreshed when a run finishes: red ("none yet"); the
# refresh made regardless of edits not yet applied: red (the typed value
# wiped); legendLeft fixed at 0.86: red.
# NEGATIVE CHECKS, 2026-11-17, the status bar (seen by David: a note ran
# over the units): the case red first for the test's own fault (a folder
# that did not exist is named by its parent, short); then the message not
# shortened: red.
# NEGATIVE CHECKS, 2026-11-17, editing in Properties: the core cases red
# against a stub (it crashed on a missing value); the window's cases passed
# at once, so: the mesh never kept: red (meshed twice); Apply never lit:
# red.
# NEGATIVE CHECKS, 2026-11-17, polish (seen by David): Properties shows the
# study when nothing is selected (its case red first: empty, then Units
# empty while the label was not yet filled); the Job dock keeps under 300
# px when it comes to the front (red against the first run's layout).
# NEGATIVE CHECKS, 2026-11-17, coloured messages (David's idea): the tone
# cases and the window's colour case written first, red. Then: no colours
# applied: the window case red; ccx's *ERROR not taken as a problem:
# test_next_step red.
# NEGATIVE CHECKS, 2026-11-17, the ember tab line: the line not drawn: the
# pixel case red.
# NEGATIVE CHECKS, 2026-11-17, the identity layer: the banner's function
# and its tests were written together; then the running state and the
# mesh problem each left unsaid: test_next_step red each.
# NEGATIVE CHECKS, 2026-11-17, reopening (seen by David): the tree said
# "not run" beside run 1's results, and the results' notes were not shown.
# Both cases written with the fix; then the job's state and the notes
# each left out: each red.
# NEGATIVE CHECKS, 2026-11-17, results in the window: the reader's cases
# and the window's results cases written first, red. Found on the way: the
# window crashed at start (the actions' states set before the results
# toolbar existed), and showed "time 0" for a run of one increment: VTK
# 9.5's HDF reader reports no time step (nor its value) for a file of one,
# so results.json now lists the time points and the window reads them
# there (a_single_time_point_keeps_its_time, red before).
# NEGATIVE CHECKS, 2026-11-17, running from the window: the run, stop and
# no-ccx cases written first, red (Run disabled for ever). Then: Stop not
# wired: the stop case red; every run in runs/1: the window case and the
# study case red. The mesh case went red once the run cases were added
# (meshing() said done before the window had taken the mesh in); meshing()
# now stays true until it has.
# NEGATIVE CHECKS, 2026-11-18, the support and load form: the 3 cases
# written first, red (no form). Then each rule broken, the test run,
# restored: an axis clicked again not turned round; the group the faces
# already are not offered (2 cases red); the normal not measured on the
# mesh; Apply lit before anything changed: each red. A support's place not
# following its kind stayed green (no case chose a load first): the case
# now chooses Move, then Fixed, and is red without the rule.
# NEGATIVE CHECKS, 2026-11-18, markers in the view: the selected item not
# lit; a force into the faces drawn from them; the markers left out of the
# model's layer: each red. Seen on the way: a force's first colour, red
# (220, 60, 60), passed the ember test, as it would pass a user's eye; now
# yellow, as Abaqus/CAE's. The face-colour cases now look at face 2 off its
# centre, where its pull's arrow starts.
# NEGATIVE CHECKS, 2026-11-18, a force along a picked edge: the case red
# (Edge disabled). Then the click not taken for an edge; the edge's
# direction not measured for the form: each red.
# NEGATIVE CHECKS, 2026-11-18, the deformed view: with viewAcross a stub
# returning the view as given, the turned-view case red, and the deform
# case red at the button not lit; the turn alone disabled, the deform case
# red at 47.2 degrees from the corner's movement (47 by hand). The view's
# grey left as it was: red.
# NEGATIVE CHECK, 2026-11-18, the sanitized build: the geometry edges read
# through netgen::OCCEdge, gf-study fails to link under GF_SANITIZE
# ("undefined reference to typeinfo for netgen::OCCEdge"), as on CI.
# NEGATIVE CHECKS, 2026-11-18, face signatures (study spec 4.2): with
# sameFace always true, describeFace and faceSignatureJson empty,
# faceSignature none, addFaceGroup and signGroup writing nothing, each of
# the setup, study, deck writer and outer surface cases red; in the window,
# a group made from picked faces unsigned, red; the group check after
# meshing left out, red; Run not held back by a changed group, red.
# NEGATIVE CHECK, 2026-11-18, every root of a STEP file: read through
# Netgen's LoadOCC_STEP (first root only), tworoots.step gave one solid,
# red; the committed meshes stay line for line the same after the change.
# NEGATIVE CHECK, 2026-11-18, the mesh's settings and kernel in its
# provenance: before they were written, the provenance case red at both;
# the committed meshes then differed at the new header lines only.
set_tests_properties(the_window_is_walked_through PROPERTIES TIMEOUT 300
    ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1;QT_QPA_PLATFORM=xcb;WAYLAND_DISPLAY=;GF_WALKTHROUGH_XVFB=1")

# Named groups from FreeCAD (docs/specs/groups-v1.md): the macro's group
# file read against GridForge's mesh, each face found by its signature.
add_executable(test_groups_file test_groups_file.cpp)
target_link_libraries(test_groups_file PRIVATE gf_mesh)
target_compile_definitions(test_groups_file PRIVATE GF_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME groups_from_freecad_are_found_by_their_faces_signatures COMMAND test_groups_file)
# GridForge's FreeCAD macro, run in FreeCAD where FreeCAD is installed (or
# its Flatpak); skipped elsewhere (CI). Its group file against the one
# written by hand from the box's faces; then GridForge reads its files.
add_test(NAME the_freecad_macro_writes_the_groups_as_specified
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/freecad/run-macro-test.sh ${CMAKE_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}/freecad-out)
set_tests_properties(the_freecad_macro_writes_the_groups_as_specified PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 300 FIXTURES_SETUP freecad_macro)
add_test(NAME groups_the_freecad_macro_wrote_are_read COMMAND test_groups_file)
set_tests_properties(groups_the_freecad_macro_wrote_are_read PROPERTIES
    FIXTURES_REQUIRED freecad_macro
    ENVIRONMENT "GF_FREECAD_OUT=${CMAKE_CURRENT_BINARY_DIR}/freecad-out")
# NEGATIVE CHECKS, 2026-11-18, groups from FreeCAD: each group-file case
# red against stubs of readGroupsFile and facesMatching; createStudy,
# addGroup and suggestedSize red against stubs; the window's case red
# against a stub newStudy, and with the offer after meshing left out. The
# macro's test red with one area of the hand-written file changed (600 to
# 601). FreeCADCmd keeps neither prints nor exit codes, so the verdict is
# a file the runner reads.

# NEGATIVE CHECKS, 2026-11-18, the AppImage's layout: the shipped toolkits
# found beside the program, red against a stub and red with both folders
# (the source tree's calculix replaced the installed one: found running
# the AppImage); the adapter finding ccx beside itself, red before.
# tools/appimage/smoke-test.sh (not in ctest: it needs the AppImage built)
# failed first on ccx's licence not carried, then on that toolkit; both
# fixed, it solves the box with the ccx it carries.

# 2026-11-18, the toolkits installed in lib/gridforge/toolkits, not
# share/ (programs: Debian policy): the toolkit-folder case red against
# the share/ path, green after.

# NEGATIVE CHECKS, 2026-11-18, the release walkthrough (ROADMAP Phase 3's
# exit): red at its first banner before the set-up steps were said; red at
# "Give the part a material" with the material left out of the banner's
# state. addMaterial and assignMaterial red against stubs. The study the
# walkthrough made, kept once and solved by the real ccx 2.23: the
# reaction on the held face 98.5095 N, the load 100 N less the 1.4905 N
# on the edge it shares with the loaded face (ccx's FORC leaves out a
# node's own load).

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
