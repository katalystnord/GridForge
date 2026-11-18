# Loads and supports: how pre-processors let the user set them up

Survey date: 2026-11-18. [gui-layouts.md](gui-layouts.md) covered where
each program puts its panes and how a face is picked; this note covers what
happens next: how a support or a load is given its kind, direction and
magnitude, and how it is shown on the model. Asked for by David after
"how would I bend the dogbone, not pull it?", with the instruction to
check every precedent, open source and proprietary.

Method: official manuals and help pages, official tutorials, and for open
source the source code itself (Qt `.ui` files and the code behind them).
Every fact has its source in brackets, listed at the end. "Not checked"
means no official source was found or opened. No program was run.

The worked case throughout: the dogbone (150 mm along x, 20 mm wide in y,
4 mm thick in z), one grip fixed, the other pushed sideways in z.

---

## 1. At a glance

| Program | Support per direction | Load direction given by | Total or per what | Drawn on the model | Steps |
|---|---|---|---|---|---|
| FreeCAD FEM | checkbox per component: unchecked free, checked a value (0 fixed) [F2] | magnitude, direction from a picked edge or planar face, Reverse checkbox; empty means normal [F1, F3] | total, spread evenly [F3] | fixed-size arrows, user scale, not by magnitude [F5] | none; amplitudes [F6] |
| PrePoMax 2.4 | per DOF: Unconstrained, Fixed, Value [P1 s5.7.4] | components F1 to F3, magnitude computed; global or user CSYS [P1 s5.7.5] | traction: total or per area; concentrated force per node [P2] | face painted in the load's colour, one arrow at the centroid; cones per held DOF; global symbol size [P3] | items per step, Propagate [P1] |
| Mecway 34 | one direction vector plus a value per constraint; three for full fixing [M1 s9.6] | global X, Y, Z only [M1 s9.13] | spread evenly over faces [M1 s9.13] | red arrowhead symbols [M1 s9.2] | none; time functions [M1] |
| AsterStudy | keywords DX, DY, DZ ticked in a form over the command catalogue [A1] | FX, FY, FZ [A1] | per area (FORCE_FACE) | mesh view shows the groups; no arrows found [A1] | not checked |
| ElmerGUI | Displacement 1 to 3, blank free [E2] | Force 1 to 3, or Normal Force; normal-tangential option [E2] | not checked | none in the manual [E1] | not checked |
| cgx | `spc 123`, `spc 1 0.1`, `spc nor 0.1` [C1] | components, per node [C1] | per node | not checked | n/a (deck) |
| ANSYS Mechanical | Displacement X, Y, Z: blank free, 0 held; Fixed Support [N2, N1] | Define By: Vector (magnitude, direction picked: face normal, edge, cylinder axis, two vertices; flip arrows in the view) or Components [N3, N4] | total, as pressure over the area [N3] | arrow in true direction, colour by kind, scoped faces coloured, Rescale [N6] | tabular per step [N3] |
| Abaqus/CAE | U1 to UR3 toggles, value default 0; presets ENCASTRE, PINNED, XSYMM ... [Q1, Q2] | CF1 to CF3; traction by a typed vector [Q3, Q4] | concentrated per node; traction per area [Q3, Q4] | yellow arrows, uniform size; held DOF a stemless arrow [Q5] | manager grid: loads by steps, Created/Propagated/Modified [Q6] |
| COMSOL | "Prescribed in x/y/z" checkboxes; unchecked free [K1] | components in a chosen coordinate system [K2] | per area, or Total force divided by area [K2] | symbol shows only that a load exists, not its direction [K3] | load groups combined into cases [K4] |
| HyperMesh | not checked | Components, or Vector (magnitude plus plane-and-vector tool); default normal [H1] | not checked | vector with "F" at the tail [H1] | not checked |
| SolidWorks Simulation | Fixed; Roller/Slider; Use Reference Geometry: per direction of a picked plane, axis or edge [W1, W2] | Normal, or Selected direction (pick a face, edge, plane or axis; components named by it) [W3] | Per item or Total by area [W3] | symbols with colour, size, preview while defining [W4] | Load Case Manager table [W5] |
| Fusion | Fixed with Ux, Uy, Uz toggles; Pin; Frictionless [U1, U2] | Normal (default on a planar face), Angle (drag manipulators), Vectors Fx, Fy, Fz; Flip Direction [U3, U4] | total by area, or per entity [U5] | face blue with a lock for Fixed [U2]; load glyph not checked | load cases in one study [U6] |
| Onshape | whole instances only [O1] | a mate connector's axes, reverse arrow [O1] | over the instance or chosen faces [O1] | arrow with the load's icon [O1] | one column per simulation [O1] |
| SimScale | Fixed support; Fixed value per direction, "unconstrained" (snippet only) [S1, S2] | components [S1] | not checked | not checked | not checked |
| Altair Inspire | fully locked by default; trident: green free, grey locked [I1] | click a face, type magnitude on the model; X, Y, Z buttons (again reverses); reverse button; Fx, Fy, Fz behind a chevron [I2] | distributed or not [I2] | red arrow, double-click to edit [I3] | current load case; table [I2] |

---

## 2. What each does that matters here

**FreeCAD FEM** [F1 to F6]. Fixed is a list of picked faces and nothing
else. Displacement has six checkable group boxes; the code reads unchecked
as free (`TaskFemConstraintDisplacement.cpp`). Force is one magnitude in N
(minimum 0), a Direction button that takes a picked planar face's normal or
an edge's line, and "Reverse direction". The direction vector is read-only
output; the main axes and datums cannot be picked as a direction, an open
request since 2019 (tracker #4064). Symbols are Coin3D glyphs of fixed
length times a user scale, sampled over the face.

**PrePoMax** [P1 to P3]. Each load window has a Region type (picked, a
surface, a node set) and limits picking to what fits while it is open. A
held DOF has three states: Unconstrained, Fixed (keeps the previous step's
deformation) and Value. Forces are components only; no picked direction
(its `AutoComputeDirection` is for pre-tension). A surface traction paints
its faces in the item's colour with one arrow at the centroid.

**ANSYS Mechanical** [N1 to N6]. The fullest direction picker: a planar
face gives its normal, a straight edge its line, a cylindrical face its
axis, two vertices the line between them. Two arrows in the lower left of
the view show both senses; clicking one flips. Switching between Vector and
Components loses what was entered. The picked direction "is not
associative": it does not follow a geometry update. Remote Force moves a
force to a point and adds its moment (Rigid, Deformable, Coupled, Beam).

**Abaqus/CAE** [Q1 to Q6]. Named support presets on top of the per-DOF
toggles. The step manager grid (loads down, steps across, each cell
Created, Propagated or Modified) is the model GridForge's Steps dock
already follows. A held DOF is drawn as an arrow without a stem; symbol
size does not follow the magnitude.

**COMSOL** [K1 to K4]. Total force is offered beside force per area, with a
warning that dividing by area is not intuitive on curved boundaries. Its
symbols do not show direction; to see a load the user computes an initial
value. That is the thing to avoid.

**SolidWorks Simulation** [W1 to W5]. "Normal" is the first choice, in the
user's words. A picked reference names the components (along edge; radial,
circumferential, axial for an axis). Reversal is by a negative value only.
Symbols preview while the load is defined; size and colour are settings.

**Fusion** [U1 to U6]. The default follows the selection: Normal for a
planar face, Angle for a curved one, Vectors for edges and vertices. Angle
can be set by dragging manipulators on the model; Flip Direction is one
button. Fixed starts with Ux, Uy, Uz all held and lets each be released.

**Altair Inspire** [I1 to I3]. The quickest: click a face, type the
magnitude in a small box on the model, Enter. X, Y and Z buttons align the
force; clicking the same button again reverses it. A support's trident
shows each axis green (free) or grey (held), clicked to toggle.

**Mecway** [M1]. A displacement constraint is one direction plus a value;
the others stay free. Simple to explain, but full fixing takes three items.

---

## 3. Patterns

1. **Two ways to give a direction, and the good tools offer both.** Typed
   components in global axes (everyone) and a direction from the model:
   normal to the face, along an axis, along a picked edge (ANSYS, FreeCAD,
   SolidWorks, Fusion, Inspire). The analyst tools lean on components; the
   designer tools lead with Normal and axis buttons.
2. **Flip is one control**: arrows in the view (ANSYS), a button (Fusion,
   Inspire), a checkbox (FreeCAD). A negative value only (SolidWorks) is the
   weakest.
3. **Supports: held by default, released per axis.** Per-axis on/off plus a
   value is universal; how "free" is shown varies (blank, unchecked,
   Unconstrained). A blank that could be read as zero is a trap; an explicit
   word ("free") or a three-state control (PrePoMax) is not.
4. **Total or per area must be said.** Tools differ (FreeCAD, ANSYS, Fusion:
   total by area; Abaqus and cgx concentrated force: per node; traction: per
   area). The label must carry it.
5. **Drawn in the true direction, at a fixed size, in the kind's colour.**
   No tool scales arrows by magnitude by default; the good ones show the
   real direction and colour the faces (ANSYS, PrePoMax). COMSOL's
   direction-less symbol is the counter-example.
6. **Picked directions do not follow the geometry** (ANSYS says so). What is
   kept is the resolved vector.
7. **Steps**: a loads-by-steps grid (Abaqus), per-step values (ANSYS), load
   cases as on/off tables (SolidWorks, Onshape, Inspire, COMSOL).
8. **Moving a load to a point with its moment** (ANSYS Remote Force, COMSOL
   Resultant) is how analysts push a grip sideways without loading one face
   unevenly. Not in GridForge's study spec yet.

---

## 4. Sources

- [F1] FreeCAD `src/Mod/Fem/Gui/TaskFemConstraintForce.ui` (main): https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Fem/Gui/TaskFemConstraintForce.ui
- [F2] `TaskFemConstraintDisplacement.ui` and `.cpp` (l.148, 438): https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Fem/Gui/TaskFemConstraintDisplacement.ui ; wiki: https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/FEM_ConstraintDisplacement.md
- [F3] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/FEM_ConstraintForce.md ; `src/Mod/Fem/App/FemConstraintForce.cpp`
- [F4] Tracker #4064: https://tracker.freecad.org/view.php?id=4064
- [F5] `src/Mod/Fem/Gui/ViewProviderFemConstraintForce.cpp` (l.79 to 110), `ViewProviderFemConstraint.cpp` (l.67), `App/FemConstraint.cpp` (l.91 to 153)
- [F6] `App/FemConstraintForce.cpp` (l.61 to 71)
- [P1] PrePoMax v2.4.0 manual, s5.7.4, s5.7.5, p6: https://prepomax.fs.um.si/wp-content/uploads/2025/10/PrePoMax-v2.4.0-manual.pdf
- [P2] https://github.com/PrePoMax/PrePoMax `Forms/55_Load/ViewLoad.cs`, `ViewSTLoad.cs` (l.30 to 111), `ViewCLoad.cs` (l.47 to 68), `FrmLoad.cs` (l.147 to 192, 358)
- [P3] same repository, `Controller.cs` (`DrawSTLoad` l.27928, `DrawFixedBCSymbols` l.27282, `DrawDisplacementRotationSymbols` l.27356), `Forms/61_Settings/ViewPreSettings.cs` (l.77 to 142)
- [M1] Mecway manual v34: https://mecway.com/manual.pdf (ch9, s9.1, s9.2, s9.6, s9.13, s9.14)
- [A1] AsterStudy user guide, "Edit command": https://gitlab.com/salomemeca/modules/salome-asterstudy/-/blob/main/docs/userguide/commands.rst ; `known_issues.rst`; `docs/assistant/intro.rst`
- [E1] ElmerGUI manual s4.6: https://www.nic.funet.fi/pub/sci/physics/elmer/doc/ElmerGUIManual.pdf
- [E2] `ElmerGUI/Application/edf/linearelasticity.xml`: https://github.com/ElmerCSC/elmerfem
- [C1] cgx 2.23 manual, s9.116 `send`: http://www.dhondt.de/cgx_2.23.pdf
- [N1] ANSYS Mechanical help v24.2, Fixed Supports: https://ansyshelp.ansys.com/public/Views/Secured/corp/v242/en/wb_sim/ds_Fixed_Supports.html (search snippet)
- [N2] Displacements: .../v242/en/wb_sim/ds_Displacements.html
- [N3] Force: .../v242/en/wb_sim/ds_Force_Load.html
- [N4] Defining Direction: .../v242/en/wb_sim/ds_definedir_graphsel.html ; Direction (v25.2): .../v252/en/wb_sim/ds_Direction.html
- [N5] Remote Force: .../v242/en/wb_sim/ds_Remote_Force_Load.html ; Pressure: ds_Pressure_Loads.html (search snippet)
- [N6] Annotations (v25.1): .../v251/en/wb_sim/ds_Annotations.html
- [Q1] Abaqus/CAE 2017, Displacement/Rotation editor: https://abaqus.uclouvain.be/English/SIMACAECAERefMap/simacae-t-lbibceditorsdisplacement.htm
- [Q2] Symmetry/Antisymmetry/Encastre: .../simacae-t-lbibceditorssymmetry.htm
- [Q3] Concentrated force: .../simacae-t-lbiloadeditorsconcforce.htm
- [Q4] Surface traction: .../simacae-t-lbiloadeditorssurfacetraction.htm
- [Q5] Arrows and colours: .../simacae-c-lbiarrowscolor.htm
- [Q6] Step-dependent managers: https://abaqus-docs.mit.edu/2017/English/SIMACAECAERefMap/simacae-c-intstepgenterms.htm , simacae-c-intstepchangestatus.htm (search snippets)
- [K1] COMSOL Structural Mechanics Module 6.1, Prescribed Displacement: https://doc.comsol.com/6.1/doc/com.comsol.help.sme/sme_ug_solid.07.056.html ; Fixed Constraint 6.3: sme_ug_solid.07.063.html (search snippet)
- [K2] Boundary Load 6.3: https://doc.comsol.com/6.3/doc/com.comsol.help.sme/sme_ug_solid.07.076.html
- [K3] Physics symbols 6.2: https://doc.comsol.com/6.2/doc/com.comsol.help.sme/sme_ug_modeling.05.032.html
- [K4] Load cases 6.4: comsol_ref_modeling.19.082.html, .19.086.html (search snippets)
- [H1] HyperMesh, Create forces: https://help.altair.com/hwdesktop/hwx/topics/pre_processing/analyze/forces_create_t.htm
- [W1] SolidWorks 2023, Fixtures: https://help.solidworks.com/2023/english/SolidWorks/cworks/IDC_HELP_RESTRAINTS.htm
- [W2] Using Reference Geometry: https://help.solidworks.com/2023/english/SolidWorks/cworks/c_Using_Reference_Geometry.htm
- [W3] Force/Torque: https://help.solidworks.com/2023/english/SolidWorks/cworks/IDC_HELP_LOAD_FORCE.htm
- [W4] Load/Fixture options: https://help.solidworks.com/2023/english/SolidWorks/cworks/IDH_HELP_PREFERENCE_LBC.htm
- [W5] Load Case Manager: https://help.solidworks.com/2023/english/SolidWorks/cworks/HIDD_LOAD_CASE_MANAGER.htm
- [U1] Fusion, Structural constraints: https://help.autodesk.com/cloudhelp/ENU/Fusion-Simulate/files/SIM-STRUCTURAL-CONSTRAINTS-CONCEPT.htm
- [U2] Tutorial, Activity 2: https://help.autodesk.com/cloudhelp/ENU/Fusion-Simulate/files/GUID-5C98694D-3AD2-4C6B-8AA2-F63DB162B052.htm
- [U3] Force load: https://help.autodesk.com/cloudhelp/ENU/Fusion-Simulate/files/SIM-FORCE-LOAD-CONCEPT.htm
- [U4] Structural Loads command: https://help.autodesk.com/cloudhelp/ENU/Fusion-Simulate/files/SIM-STRUCTURAL-LOADS-CMD.htm
- [U5] Structural Loads dialog: https://help.autodesk.com/cloudhelp/ENU/Fusion-Simulate/files/SIM-STRUCTURAL-LOADS-DIALOG.htm
- [U6] Load cases: https://help.autodesk.com/cloudhelp/ENU/Fusion-Simulate/files/SIM-LOAD-CASES-CONCEPT.htm
- [O1] Onshape Simulation: https://cad.onshape.com/help/Content/simulation.htm
- [S1] SimScale crane tutorial: https://www.simscale.com/docs/tutorials/linear-static-analysis-of-a-crane-2/ ; bracket: https://www.simscale.com/tutorial-stress-analysis-of-a-bracket/
- [S2] Fixed value (search snippet only; page 404 when fetched): https://www.simscale.com/docs/simulation-setup/boundary-conditions/fixed-value-boundary-condition-type/
- [I1] Altair Inspire, Supports: https://help.altair.com/inspire/en_us/topics/inspire/structure/supports_inspire_c.htm
- [I2] Forces: https://help.altair.com/inspire/en_us/topics/inspire/structure/force_inspire_c.htm
- [I3] Getting started tutorial: https://help.altair.com/inspire/en_us/topics/inspire/tutorials/tut_GSinspire_overview_c.htm

Not reached: Simcenter 3D / NX (help pages not found), Altair SimLab, Ansys
Discovery, Inventor Stress Analysis.
