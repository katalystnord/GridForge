# GUI layouts: how pre-processors arrange their windows

Survey date: 2026-11-16. The 2026-11-14 survey ([README.md](README.md),
[landscape.md](landscape.md)) covered features; this note covers window
layout only. Method: official manuals (PDF text and their figures, looked at
page by page), official online help, vendor pages and, for ParaView and
tomviz, the main-window `.ui` and `.cxx` files in their own source trees.
Every fact has its source in brackets, listed at the end as [S1] to [S25].
"Not checked" means no official source was found or opened. No program was
downloaded or run; the ParaView and FreeCAD Flatpaks on this machine were
inspected as files only.

Bias declared up front: David prefers ParaView's GUI to FreeCAD's. Both run
on Qt 6.11 here (Flatpak runtime `org.kde.Platform/6.11` and `org.kde.Sdk/6.11`,
from `flatpak info`); FreeCAD ships its own Qt stylesheets
(`share/Gui/Stylesheets/FreeCAD.qss`, `defaults.qss`, `overlay/*.qss` in the
Flatpak), ParaView's components carry a `fusion` style name (`strings` on
`libpqApplicationComponents-pv6.2.so`). That ParaView runs with the Fusion
style by default is from the brief, not checked further.

---

## 1. At a glance

Positions are the default layout as documented. "Dock" means the pane is a
movable, closable dock (Qt `QDockWidget` or the vendor's equivalent).

| Program | Model tree | Property editor | Viewport | Toolbars | Steps | Job monitor | Log | Panes dock? |
|---|---|---|---|---|---|---|---|---|
| PrePoMax 2.4 | left, 3 tabs: Geometry, FE Model, Results [S1 p3, p5] | modal feature window per item [S1 p6] | centre [S1 p3] | 3 rows at top, menus switch with tab [S1 p6] | `Steps` container in the FE Model tree; separate `Analyses` container [S1 p5] | separate Monitor window [S1 s5.8] | message area, bottom [S1 s3.4] | not checked |
| FreeCAD 1.1 FEM | Model panel (tree above, property editor below), left [S3, S4] | lower half of Model panel [S4] | centre, tabbed [S3] | rows at top, per workbench [S3] | none: one analysis container [S7]; one step per analysis (landscape.md) | Solver job control task panel [S9] | Report view, bottom [S3] | yes; Tasks is its own dock since 1.0 [S4, S5] |
| Abaqus/CAE | left, tabs Model and Results [S10, S11] | modal editors from managers [S13] | centre: canvas with viewports [S10] | top; module toolbox as a column beside the tree [S11] | `Steps` in tree; step-dependent managers: table of objects by steps [S13, S14] | Job Manager, then Job Monitor dialog [S12] | message area, bottom, shares space with CLI [S10] | not checked |
| ANSYS Mechanical | Outline, top left [S15] | Details pane, below Outline [S15] | Geometry window, centre [S15] | ribbon with tabs [S15] | Analysis Settings; per-step values in Tabular Data and Graph windows under the viewport [S19] | `Solution Information` object in the Outline; progress in status bar [S21] | Messages window, hidden by default [S20] | yes: move, tab-dock, auto-hide, Reset Layout [S16] |
| Mecway 34 | outline tree, left [S22 s2.1] | Analysis and items edited from context menu [S22 s2.1, s2.2] | graphics area, "most of the window" [S22 s2.1] | not checked | none in GUI; configurations as tabs above the tree; time via tables/formulas; extra CCX steps as raw text [S22 s2.8, ch9, s13.3] | solver window [S22 s10.3] | not checked | not checked |
| SALOME 9.16 | Object Browser [S23a] | per-operation dialogs [S23b] | viewer windows, splittable and tabbed [S23a] | context-sensitive, movable [S23a] | (module-specific; see AsterStudy) | (module-specific) | Python console dock [S23a] | yes: "separate dockable windows" [S23a] |
| AsterStudy | left column: Case View and History View tabs; Data Settings and Data Files boxes [S24] | not checked | not checked | not checked | stages under the case [S24] | History View, run with Run Parameters box [S24] | not checked | not checked |
| ElmerGUI | Object Browser, left, two columns Object and Value [S25 fig 4.2] | separate windows (Equation, Material, BC) [S25 s4.2, s4.6] | right of browser [S25 fig 4.2] | one row [S25 fig 4.2] | none (one Simulation setup) | Solver Log Window and convergence monitor [S25 s7] | Solver Log Window [S25 s7] | not checked |
| CMB modelbuilder | entity tree under a `Model` tab [S26] | tabbed docks on the right: Display, Color Map, Model, Mesh, Info, Attribute [S26, S27] | left middle [S26] | top [S26] | attribute templates (no step model) [S26] | not covered | Log Window, bottom dock [S26, S27] | yes (float and close buttons on each pane in the screenshot) [S27] |
| tomviz 3.1 | `Pipelines` dock, left [S29, S31] | `Properties` dock, left [S31] | ParaView's tabbed multi-view, with a histogram above it [S32] | Visualization Modules, Utilities, ParaView's VCR [S31] | none | none | Messages and Python Console, bottom [S31] | yes, `QDockWidget` throughout [S31] |
| ParaView 6.2 | Pipeline Browser, left [S34] | Properties, left, tabbed with Information (and View, Display if split) [S35] | layout tabs, split views [S36] | several rows at top [S33] (three in David's screenshot) | n/a (time steps: Time Manager, VCR) | n/a | Output Messages, bottom, hidden by default [S35] | yes [S33, S34] |

---

## 2. Program by program

### PrePoMax 2.4.0 (manual, October 2025) [S1]

| Element | What the manual shows or says |
|---|---|
| Window (Fig. 1, p3) | Menus and three toolbar rows across the top; tree on the left; 3D view in the centre with legend top left and a results box top right; message area under the view; status bar with the unit system ("Unit system: mm, ton, s, °C") at the bottom right. |
| Tree (Fig. 2, p5) | Three tabs, Geometry, FE Model, Results, "dictate the workflow"; "Each tab also activates proper menus in the top toolbar." A search bar sits on top of each tab (p6). FE Model tree: Model, Mesh (Parts, Node Sets, Element Sets, Surfaces), Features, Materials, Sections, Constraints, Contacts, Distributions, Amplitudes, Initial Conditions, Steps, Analyses. |
| Two ways in | "use options available in menus (menu bar and top toolbar) or select them in the model tree" (p6). |
| Applying a BC | Steps 7 to 9 of the workflow: create a step, then BCs and loads under it. Preferred: no set first; "the selection tool within the feature window will let the user pick entities" (p6). The Set Selection window (Fig. 12, p25) offers geometry-based picking (surfaces/edges/vertices, part, edge angle, surface angle; by location or by ID) and mesh-based picking, Shift adds, Ctrl removes. |
| Steps on screen | A "Symbols" drop-down below the menu bar chooses which symbols are drawn: None, Model, Step-1, ... (s5.7.5). Items can be deactivated without deletion (same section). |
| Run and watch | Analysis menu: Run, Check Model, Monitor ("open the window that displays solver output, job status, convergence information and dat file contents"), Results, Kill (s5.8). |
| Results | Results tab; a Results toolbar with result file, deformation variable and scale, first/previous/next/last increment, increment list, Animate (Fig. 25, 26, p59). |
| Docking | Not stated in the manual: not checked. |

### FreeCAD 1.1 FEM workbench [S2 to S9]

wiki.freecad.org is behind a bot wall (as in landscape.md); the pages were read
in FreeCAD's own wiki mirror on GitHub.

| Element | Source says |
|---|---|
| Window (Interface diagram, v0.19) | 11 parts: main view area (tabbed), 3D view, Combo view upper (tree) and lower (property editor), Selection view, Report view, Python console, status bar, toolbars, workbench selector, menu [S3]. The diagram puts Combo view and Selection view on the left, Report view and Python console along the bottom. |
| Panels | Since 1.0 the Task panel "is a separate panel called Tasks", dockable on top of the Model panel [S4]; it holds the active tool's widgets and "normally opens when a tool that requires user input is activated" [S5]. All panels except the 3D view toggle from View, Panels [S3]. |
| Analysis | "It is mandatory to have an analysis container that holds all the needed objects"; objects join it by drag and drop; double-click activates it [S7]. |
| Applying a BC | Start the command, press Add, "In the 3D view select the object the boundary condition should be applied to" (vertex, edge or face) [S8]. The object then sits in the analysis in the tree. |
| Run | Select the solver object, Solver job control: edit working directory, analysis type, "Write .inp file", optionally "Edit .inp file", "Run CalculiX" [S9]. The window freezes while ccx runs (#12706, in landscape.md). |
| Results | Results menu: Show result, Post pipeline from result, VTK filters (cut, clip, data at point) [S6]. |
| Steps | One step per analysis (#11639, landscape.md). |
| Styling | Own QSS stylesheets, including an overlay set (Flatpak files, see header). |

### Abaqus/CAE [S10 to S14]

The 3DS help needs a login; the UCLouvain and Colorado mirrors of the official
User's Guide were used. The release on the UCLouvain pages is not stated
(page metadata says copyright 2012); Colorado's path says v2016.

| Element | Source says |
|---|---|
| Window (main-window figure) | Title bar; menu bar; toolbars; context bar (Module, Model, Part lists) right of the toolbars; Model Tree / Results Tree as tabs on the left; toolbox area as a narrow column between tree and canvas; canvas with floating viewport windows; prompt area under the canvas; message area or command line at the bottom [S10, S11]. |
| Modules | "the Module list in the context bar allows you to move between these modules"; menus and toolbox change with the module [S10]. |
| Steps | `Steps` is a tree container [S11]. Step-dependent managers: "an alphabetical list of existing loads along the left side", "The names of all the steps in the analysis appear along the top", and the table "displays the status of each load in each step" [S13]. Statuses seen in the tutorial: Created, Propagated, Modified [S14]. |
| Job | Job Manager: Monitor, Results, Kill. Job Monitor: "The top half ... displays the information available in the status file"; tabs below: Log, Errors, Warnings, Output, Data File, Message File, Status File [S12]. |
| Docking | Not stated: not checked. |

### ANSYS Mechanical 2026 R1 [S15 to S21]

| Element | Source says |
|---|---|
| Window (Interface Overview figure, 2024 R2) | Ribbon with tabs (Home, Model, Display, Selection, Automation) and a contextual tab; graphics toolbar over the view; Outline top left; Details pane below it; Geometry window centre; status bar with messages, selection and unit system ("Metric (mm, kg, N, s, mV, mA)") [S15]. |
| Tree | Project, Model (Geometry, Materials, Coordinate Systems, Connections, Mesh), the analysis (Static Structural) with Analysis Settings, Solution, Solution Information [S15]. Context menu: Insert, Solve, Duplicate, Rename, Group [S17]. "Selecting an object in the Outline displays attributes and controls ... in the Details pane" [S17]. |
| Details | Categories that expand and collapse; undefined or invalid fields "highlighted in yellow"; scoping fields that need a selection and an Apply [S18]. |
| Steps | "Inserting a boundary condition updates the Tabular Data window with an entry table where you can enter the magnitude ... on a per-step basis. The Graph window plots these values"; the two windows "display beneath the Geometry window" [S19]. |
| Run and watch | "a progress pane is displayed in the status bar"; detail in "the Solution Information object contained under the Solution folder": solver output, convergence, Newton-Raphson residuals [S21]. |
| Messages | Hidden by default; the status bar flags new messages; a message's object is highlighted in the tree [S20]. |
| Docking | "move, resize, tab-dock, and auto-hide window panes"; drag a title bar to see dock targets; "Reset Layout" restores the default [S16]. |

### Mecway 34 [S22]

| Element | Manual says |
|---|---|
| Window | Mesh "displayed in the graphics area which occupies most of the Mecway window"; "the outline tree on the left hand side"; items edited by right-click (s2.1). |
| Tree | Analysis, Geometry, Components and Materials, Loads and Constraints, Named Selections, Solution (s2.2 to s2.7). |
| Applying a load | "right click the Loads & constraints branch ... If you select some of these items before you create the new load ... it will automatically use them" (ch9). |
| Steps | No step manager. Configurations: separate load sets, chosen by "the tabs above the outline tree" (s2.8). Time variation by table or formula (ch9). With CCX, "Custom step contents" adds raw text, including "additional step sections" (s13.3). |
| Run | A solver window or monitor shows progress messages; "Saved monitor" keeps them with the model (s10.3). |
| Units | Per-field unit drop-down that converts the value (s2.9). |
| Docking, toolbars | Not checked. |

### SALOME 9.16 (Shaper, SMESH) and AsterStudy [S23, S24]

| Element | Source says |
|---|---|
| Desktop | "a single Desktop top-level window"; "Object Browser, Python Console, MenuBar, etc are, in fact, separate dockable windows"; they can be moved, hidden "or even place[d] outside desktop", and positions are saved [S23a]. |
| Viewers | Split vertically or horizontally, stacked in tabs [S23a]. |
| Modules | Each module "add[s] their own menu items, buttons in toolbar, windows" [S23b]. |
| Object Browser | "displaying the structure of the current study in a tree-like form", extra columns (Entry, IOR, Value) hidden by default [S23c]. Default position not stated: not checked. |
| AsterStudy | Left column with tabs Case View and History View; "In the Data Settings box, right-click CurrentCase and choose Add Stage"; run from History View after setting the Run Parameters box (time, memory, version); results opened from Data Files, post-processed in ParaVis [S24]. |

### ElmerGUI (manual built 2026-01-20) [S25]

| Element | Manual says |
|---|---|
| Window (Fig. 4.2) | Object Browser on the left with columns Object and Value; `[Add...]` labels next to Equation, Material, Body force, Initial condition, Boundary condition; 3D view to the right. |
| Editing | Each item opens its own window (Equation Window, Material Window, Boundary Condition Window). |
| Applying a BC | Alt (or AltGr) and double-click a surface, or double-click with "Set boundary properties" pressed, opens the Boundary Property Editor (s4.6). |
| Physics panels | Defined by XML ed-files in `ELMERGUI_HOME/edf` (Appendix C). |
| Run | Sif, Generate; Run, Start solver; "there is a log window and a convergence monitor"; Run, Kill solver; one toolbar button for "Generate, save and run" (s7). |
| Results | Run, Start ParaView (s7). |

### CMB modelbuilder (Kitware) [S26 to S28]

The getting-started page describes an older modelbuilder; the SMTK panel
reference [S28] is current. Default placement of the current panels: not checked.

| Element | Source says |
|---|---|
| Window (WindowLayout.png) | Toolbar on top; model view "left middle"; "The right middle portion of the window is menu tab which controls the model details"; "The lower portion of the window is the log window" [S26]. The screenshot shows tabs Display, Color Map, Model, Mesh, Info on the right, each pane with float and close buttons, and a ParaView-style Properties layout (search box, Coloring, Styling, Lighting, Transforming) [S27]. |
| Model tree | "pick them in the entity tree under 'Model' tab" [S26]. |
| Attributes (the BCs) | "The attribute view is customized by the template file"; export by File, Export Simulation Files, via a Python script [S26]. |
| SMTK panels (ParaView plugins) | Resource panel (tree of resources and components), Attribute panel, Operation toolbox (searchable list), Operation parameter editor; toolbox and editor are separate panels so users on small displays can arrange them [S28]. |

### tomviz 3.1 (Kitware, built on ParaView) [S29 to S32]

| Element | Source says |
|---|---|
| Build | `find_package(ParaView REQUIRED)` [S30]; ships "ParaView 6.1.1 with Qt 6" [S29]. |
| Docks (MainWindow.ui) | Pipelines (left), Properties (left), Lights Inspector (left), Animation (bottom), Messages (bottom, ParaView's `pqOutputWidget`), Python Console (bottom, `pqPythonShell`); ParaView's `pqVCRToolbar` [S31]. |
| Centre (CentralWidget.ui) | A vertical splitter: histogram and transfer function on top, ParaView's `pqTabbedMultiViewWidget` below [S32]. |
| Flow | "the pipeline is at the top left"; "Click a node card ... show its properties in the Properties panel" [S29, S29b]. |

The brief adds David's screenshot of tomviz 3.1.0 beside ParaView 6.2.0: the
same arrangement as above, which the `.ui` files confirm.

### ParaView 6.2 (the reference) [S33 to S36]

| Element | Source says |
|---|---|
| Window | "The UI consists of of menus, dockable panels, toolbars, and the viewport" [S33]. |
| Docks (ParaViewMainWindow.ui/.cxx) | Left: Pipeline Browser, then Properties tabbed with View, Display and Information (`tabifyDockWidget`); tab position North. Bottom: Time Manager tabbed with Statistics, Output Messages, Python Shell, all hidden at start. Right: Color Map Editor, Find Data, Light, Memory, MultiBlock inspectors, hidden. Default panel mode ALL_IN_ONE: Properties, Display and View as sections of one panel [S35]. |
| Information | "tucked under a tab below the Properties panel" [S34]. |
| Flow | Create a source, it appears in the Pipeline Browser; "the Apply button becomes enabled and highlighted"; eyeball icons toggle visibility; Auto Apply as a setting and a toolbar button [S33]. |
| Views | Split horizontally or vertically; drag a view's title bar to swap; layouts in tabs; pop a tab out; fullscreen [S36]. |

---

## 3. The main flow, side by side

| Program | Pick a face | Apply load or support | Seen in tree | Run | Progress | Results |
|---|---|---|---|---|---|---|
| PrePoMax | in the view, from the feature window's Set Selection | feature window (modal) | under the step | Analysis, Run | Monitor window | Results tab, same view |
| FreeCAD | 3D view after Add | Tasks panel | in the analysis container | Solver job control | window frozen (#12706) | post pipeline objects |
| Abaqus/CAE | viewport, prompt area guides | editor from a manager | under Steps or Loads | Job Manager | Job Monitor dialog | Results Tree tab |
| ANSYS | Geometry window, then Apply in Details | Details pane (not modal) | Outline | Solve on ribbon or context menu | status bar, Solution Information | under Solution in the Outline |
| Mecway | graphics area, before or after | context menu dialog | Loads & constraints | not checked | solver window | under Solution in the tree |
| ElmerGUI | Alt + double-click a surface | separate window | Object Browser | Start solver | log window, convergence monitor | launches ParaView |
| CMB | view or Model tab | Attribute tab | not checked | Export Simulation Files | n/a | n/a |
| ParaView | n/a (selections for queries) | Properties + Apply | Pipeline Browser | n/a | n/a | same window |

---

## 4. What the survey shows

1. **Tree left, properties under or beside it, view centre, log bottom** is
   the shape almost everyone uses: PrePoMax, FreeCAD, Abaqus, ANSYS, Mecway,
   ElmerGUI, ParaView, tomviz. ANSYS's default (Outline over Details, both
   docks) is ParaView's (Pipeline Browser over Properties).
2. **Non-modal property editing wins on flow**: ANSYS Details and ParaView
   Properties edit in place; PrePoMax, Abaqus and ElmerGUI open a window per
   item. FreeCAD's Tasks panel is a dock, but it exists only while a command
   is active and holds that command's widgets [S5, S8].
3. **Steps have three visual forms**: a tree container (PrePoMax, Abaqus),
   an objects-by-steps table (Abaqus managers), per-step values in a table and
   graph under the view (ANSYS). Only PrePoMax shows which step's symbols are
   drawn (its Symbols drop-down).
4. **The job is either a tree object (ANSYS Solution Information, PrePoMax
   Analyses) or a dialog (Abaqus, Mecway, ElmerGUI).** Status-bar progress
   (ANSYS) lets the window stay usable.
5. **Mode switching** is done by tree tabs (PrePoMax, Abaqus Model/Results)
   or a module list (Abaqus, SALOME). ParaView, ANSYS and tomviz have no modes.
6. **Units in the status bar**: PrePoMax and ANSYS both.
7. **Docking**: documented for ANSYS, SALOME, FreeCAD, ParaView, tomviz,
   CMB; not found for PrePoMax, Abaqus, Mecway, ElmerGUI.

---

## 5. What GridForge takes

Built from ParaView's own components where they fit (as tomviz is): Qt
`QDockWidget` docks, ParaView's tabbing of docks, plain Fusion, no QSS.

### Proposal A (chosen by David, 2026-11-17): ParaView's layout, one study tree

David, 2026-11-17: "Definitely alternative A", with icons, borrowing
heavily from ParaView. Since this survey: Python became an optional
toolkit (a console dock once it exists), and the mesh is GridForge's own
(gf_mesh, Netgen); the drawing at https://claude.ai/artifact/Xk83LLMjzSw4XAQfen22pt
shows A with the dogbone study in it.

```
+--------------------------------------------------------------------------------+
| File Edit View Study Mesh Run Results Help                                     |
| [open save][undo][Check Run Kill] [pick: Face Edge Vertex Part] [camera] [|<>|] |
| Step: [Step-2 (static) v]   Show symbols for: [Step-2 v]   Units: mm t s MPa   |
+---------------------------+----------------------------------------------------+
| Study                  [x]| [ Model ] [ Results ] [ + ]                       |
|  v Geometry  bracket.brep | +------------------------------------------------+ |
|  v Mesh       (Gmsh)      | |                                                | |
|  v Materials  S355        | |                                                | |
|  v Sets       fixed, load | |          3D view (VTK)                         | |
|  v Steps                  | |   picked faces highlighted;                    | |
|     v Step-1 static  o    | |   BC and load symbols of the chosen step       | |
|        Fixed: fixed       | |                                                | |
|        Pressure: load     | |                                                | |
|     > Step-2 static  o    | +------------------------------------------------+ |
|  v Jobs  Job-1  running   | split / maximise / close per view, as ParaView     |
|  v Results  Job-1.frd     |                                                    |
+---------------------------+                                                    |
| Properties | Information  |                                                    |
| [Apply] [Reset] [Delete] ?|                                                    |
| search [.............]  * |                                                    |
| v Pressure                |                                                    |
|   Region  [2 faces][Pick] |                                                    |
|   Value   [10] [MPa v]    |                                                    |
|   Amplitude [ramp v]      |                                                    |
| v Display                 |                                                    |
+---------------------------+----------------------------------------------------+
| Messages | Job (log, increments, convergence) | Steps table | Deck (.inp)      |
+--------------------------------------------------------------------------------+
| Ready                          Job-1: step 2, increment 14/??   mm, t, s, MPa  |
+--------------------------------------------------------------------------------+
```

| Choice | Taken from |
|---|---|
| Study tree top left, Properties tabbed with Information below it, both docks | ParaView's dock layout [S35], tomviz [S31], ANSYS Outline over Details [S15] |
| One tree for geometry to results, steps as containers holding their BCs and loads | PrePoMax FE Model tree [S1 p5], Abaqus Steps [S11], Mecway's Solution branch [S22] |
| Edit in the Properties dock, never a modal window; Apply, Reset, Delete, Help, search, advanced (gear) toggle | ParaView Properties and Apply [S33]; search and gear in CMB's ParaView-style panel [S27]; ANSYS Details [S18]; against PrePoMax and ElmerGUI windows |
| Region field with a Pick button: picking in the view fills it; pick modes Face, Edge, Vertex, Part, by angle | ANSYS scoping and Apply [S18]; PrePoMax Set Selection, Shift adds, Ctrl removes [S1 p25]; FreeCAD Add [S8] |
| Unfilled or invalid fields marked, and the item marked in the tree | ANSYS yellow fields and highlighted messages [S18, S20] |
| Step selector and "Show symbols for" in a toolbar | PrePoMax Symbols drop-down [S1 s5.7.5] |
| Steps table dock: rows are loads and BCs, columns steps, cells Created, Propagated, Modified, Inactive | Abaqus step-dependent managers [S13, S14] |
| Jobs in the tree, with a Job dock for log, increments and convergence; progress in the status bar; window never blocks | ANSYS Solution Information and status bar [S21]; PrePoMax Monitor [S1 s5.8]; ElmerGUI log and convergence monitor [S25]; Abaqus Job Monitor tabs [S12]; tenet 7 |
| Deck dock showing the generated `.inp` | FreeCAD "Edit .inp file" [S9]; ElmerGUI Sif, Edit [S25]; survey item 6 |
| Layout tabs Model and Results in the centre, split views | ParaView layouts [S36]; tomviz's `pqTabbedMultiViewWidget` [S32] |
| Units in the status bar and beside every value | PrePoMax status bar [S1 p3]; ANSYS status bar [S15]; Mecway unit drop-downs [S22 s2.9] |
| VCR toolbar for increments in Results | ParaView and tomviz `pqVCRToolbar` [S31] |
| Docks movable, closable, layout saved, View, Reset Layout | ANSYS Reset Layout [S16]; SALOME saved positions [S23a] |
| No Python shell dock (Python is out of process, per the toolkits decision) | CLAUDE.md; ParaView's shell is the counter-example [S35] |

### Alternative B: workflow tabs over the tree (PrePoMax, Abaqus)

```
+------------------------------------------------------------------+
| menus and toolbars change with the tab                           |
+--------------------------+---------------------------------------+
| [Geometry][Model][Results]                                       |
|  tree for that phase     |          3D view                      |
+--------------------------+                                       |
| Properties               |                                       |
+--------------------------+---------------------------------------+
| Messages | Job                                                   |
+------------------------------------------------------------------+
```

Taken from PrePoMax's three tabs that "dictate the workflow" and switch
menus [S1 p5, p6] and Abaqus's Model/Results tabs and module list [S10, S11].
Gains: a newcomer sees one phase at a time. Costs: modes hide state, and
neither ParaView nor ANSYS needs them; a Mesh edit while on Results means a
tab switch. Keep as a fallback if testers get lost in one long tree.

### Alternative C: details beside the view, steps under it (ANSYS, CMB)

```
+------------------------------------------------------------------+
| ribbon-less toolbars                                             |
+-------------+------------------------------------+---------------+
| Study tree  |          3D view                   | Properties    |
|             |                                    | Information   |
|             +------------------------------------+               |
|             | Steps: table and load-time graph   |               |
+-------------+------------------------------------+---------------+
| Messages | Job                                                   |
+------------------------------------------------------------------+
```

Taken from ANSYS's Tabular Data and Graph windows under the view [S19] and
CMB's property tabs on the right [S26, S27]. Gains: wide screens; per-step
values always in sight. Costs: three columns crowd a laptop; the view
shrinks from both sides. Because every pane in A is a dock, C is a
rearrangement of A that a user can make, not a second program.

### Not taken

- FreeCAD's Tasks dock that appears per command [S5, S8]: properties live in one
  permanent dock instead.
- One modal window per item (PrePoMax, ElmerGUI, Abaqus editors).
- A ribbon (ANSYS): not the ParaView look.
- An embedded Python console (ParaView, SALOME, Abaqus): against the
  out-of-process API decision.

---

## 5b. CMB in depth: a pre-processor inside ParaView

CMB's modelbuilder is ParaView's client with SMTK plugins: the closest
existing thing to GridForge's position. Read from source (CMB, SMTK and aeva
clones, commits in [S37] to [S39]); no program was built or run.

**Window.** The default dock layout is code, not a `.ui` file
(`modelbuilder/mbLayoutSpec.cxx`): left, SMTK's attribute editor, ParaView's
Properties and Information, tabbed together, 400 px wide; right, the SMTK
resource tree, 200 px; bottom, ParaView's Output Messages, hidden [S37a].
Tabs sit North on both sides (`cmbMainWindow.cxx` line 232). ParaView's
Pipeline Browser, Sources, Filters and Animation are hidden until the user
turns on a "post-processing mode" (`togglePostProcessingMode`, lines 437 to
520) [S37b]. So CMB is a ParaView with the pipeline put away, and the
attribute editor takes the Properties slot.

**Attributes from templates.** All simulation input is an SMTK *attribute
resource* described by an XML template (`.sbt`): a *Definition* fixes the
named *Items* (Double, Int, String, references, with defaults, ranges,
enumerations, units such as `Units="meters"` [S38i]) and may inherit a base type ("deflection, temperature,
and voltage boundary condition definitions might all inherit a Dirichlet
boundary condition definition"); an *Attribute* is one instance of it
[S38a]. The editing panels "are automatically produced by SMTK at runtime,
based on the XML file(s)"; default values show on a yellow background,
changed values on white [S38a]. Items can be marked advanced
(`AdvanceLevels`) and filtered by *categories* [S38b].

**Boundary conditions on model entities.** An attribute is *associated* with
geometric model entities through an `AssociationsDef`, which limits what may
be picked (`MembershipMask` edge, or `<Accepts>` with a filter such as
`face`) and how many [S38b]. SMTK's own `elasticity.sbt` is a small solid
mechanics example: an `elasticity` material (Poisson's ratio limited to
-1 to 0.5, Young's modulus), a `constraint` on faces with a direction enum
(All, X, Y, Z, XY, XZ, YZ) and a `load` on faces with three force
components [S38c]. The *Attribute View* lists the attributes of the chosen
types with New, Copy and Delete buttons, a search box, and an association
widget with *Available* and *Current* lists (title "Model Boundary and
Attribute Associations"); `RequireAllAssociated` makes an unassigned entity
invalid [S38b].

**Analyses, not steps.** An SMTK *analysis* is "a resource of categories":
choosing an analysis (or a sub-analysis; `Exclusive` forces one) switches
on the categories, and so the items, that it needs [S38b]. It selects what
physics is set up; it is not a load step. No step or load-history concept
was found in the attribute, simulation or task chapters (grep for "step");
time appears only as ordinary items of a template, for example
`nsteps` and `term` in a `simulationtime` definition in
`HydraTemplateV1.sbt` [S38d]. A *Group View* lays child views out tiled,
tabbed or in collapsible group boxes; `elasticity.sbt`'s top level is a
tabbed group of Elasticity, Constraints and Loads [S38b, S38c].

**Operations.** Everything that changes a resource is an operation whose
parameters are themselves an attribute. The *operation toolbox* is a
searchable grid of buttons filtered by the current selection; a click runs
an operation that needs nothing beyond its associations, a long click opens
the *parameter editor*, a separate panel "to allow users with small displays
... arrange them separately"; Ctrl+Space focuses the search [S38e]. aeva's
guide describes the same panel with a "limit by selection" checkbox and
toolbar shortcuts [S39].

**Export to a solver.** File, Export Simulation... runs a user-chosen Python
file: `pqSMTKExportSimulationBehavior.cxx` adds that action to the File menu,
opens a file dialog for `*.py`,
imports it as an operation, shows its parameters in a modal "Simulation
Export Dialog", runs it, and reports "Export failed. Please see output log"
on failure [S38f]. The script gets an `ExportSpec` (simulation attributes,
an attribute resource of file locations, and an object for querying the
model or mesh) and writes "whatever files are needed to run the simulation";
Kitware recommends Python for this "than a large C++ code base to write what
is usually a flat text file" [S38g, S40]. Job submission is a *task* in
SMTK's task system (task graph with dependencies and states, examples
including "exporting a simulation input deck, submitting a simulation job")
[S38h]; a CMB job monitor was not found in these trees: not checked.

**What this implies for GridForge.**

1. *Study tree*: CMB splits geometry (resource tree, right) from simulation
   data (attribute editor, left). Proposal A's one tree is the opposite
   choice; CMB's evidence is that the split works when the data has no
   steps. GridForge has steps, so keep one tree with BCs under steps.
2. *Properties dock*: SMTK confirms the pattern: generated from a
   declarative definition, defaults visibly different from edited values,
   advanced items hidden, a search box, invalid items flagged, an
   association field with Available and Current. GridForge's study-file
   items should carry the same metadata (label, unit, range, enum,
   default, advanced, which entity kinds may be assigned) so one generic
   editor serves CalculiX and OpenFOAM® alike; that fits the toolkit
   manifest door better than hand-built forms. SMTK's code itself is not
   taken (tenet 3; and its panels need ParaView's `pqApplicationCore`
   [S38e]).
3. *Steps table*: SMTK has nothing to copy here; Abaqus's objects-by-steps
   table stays the model. An SMTK-style *analysis* (physics selection that
   hides irrelevant fields) is a separate, useful idea for a step's
   procedure (static, frequency, heat) choosing which items show.
4. *Operations*: the selection-filtered, searchable toolbox with a separate
   parameter panel is a good fit for mesh and set operations, and the long
   click "edit before run" is worth keeping.
5. *Export*: CMB delegates the deck to a Python script in a modal dialog.
   GridForge writes the `.inp` itself, keeps it, and shows it in the Deck
   dock (tenets 9, 10); the toolkit door is the out-of-process counterpart of
   CMB's export script.

---

## 6. Not checked

PrePoMax, Abaqus, Mecway and ElmerGUI: whether panes dock. Mecway toolbars
and solver window layout. SALOME's default dock positions and the SMESH
operation dialogs' placement. AsterStudy beyond the left column and Run
Parameters box. Abaqus/CAE release of the UCLouvain mirror. ParaView's
Fusion default (from the brief). Kitware's simulation-workflows repository
of real templates and export scripts (https://gitlab.kitware.com/cmb/simulation-workflows
returned 403). A job monitor in CMB. No program was run.

---

## 7. Sources

- [S1] PrePoMax 2.4.0 manual v1.0.0, 2025-10-15: https://prepomax.fs.um.si/wp-content/uploads/2025/10/PrePoMax-v2.4.0-manual.pdf (Fig. 1 p3, Fig. 2 p5, p6, Fig. 12 p25, s5.7.5, s5.8, Fig. 25-26)
- [S2] FreeCAD wiki mirror: https://github.com/FreeCAD/FreeCAD-documentation/tree/main/wiki
- [S3] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/Interface.md (and its figure `images/FreeCAD_interface_base_divisions.svg`)
- [S4] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/Combo_view.md
- [S5] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/Task_panel.md
- [S6] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/FEM_Workbench.md
- [S7] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/FEM_Analysis.md
- [S8] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/FEM_ConstraintFixed.md
- [S9] https://github.com/FreeCAD/FreeCAD-documentation/blob/main/wiki/FEM_SolverControl.md
- [S10] Abaqus/CAE, Components of the main window: https://abaqus.uclouvain.be/English/SIMACAEGSARefMap/simagsa-c-absmaincaewindow.htm
- [S11] Its figure: https://abaqus.uclouvain.be/English/SIMACAERefImages/gst-main-window-nls.png
- [S12] Monitoring a job: https://abaqus.uclouvain.be/English/SIMACAECAERefMap/simacae-c-anaconcmonitor.htm
- [S13] Step-dependent managers (v2016): https://ceae-server.colorado.edu/v2016/books/usi/pt01ch03s04s02.html
- [S14] Boundary Condition Manager tutorial: https://abaqus.uclouvain.be/English/SIMACAEGSARefMap/simagsa-t-caehingemodbcpin.htm
- [S15] ANSYS Mechanical Interface Overview (2024 R2) and figure: https://ansyshelp.ansys.com/public/Views/Secured/corp/v242/en/wb_sim/ds_Window.html, `graphics/gds_UI_Overview.png`
- [S16] Windows Management (2026 R1): https://ansyshelp.ansys.com/public/Views/Secured/corp/v261/en/wb_sim/ds_wind_mgt.html
- [S17] Outline: https://ansyshelp.ansys.com/public/Views/Secured/corp/v261/en/wb_sim/ds_Tree_Outline.html
- [S18] Details Pane: https://ansyshelp.ansys.com/public/Views/Secured/corp/v261/en/wb_sim/ds_Details_View.html
- [S19] Graph and Tabular Data: https://ansyshelp.ansys.com/public/Views/Secured/corp/v261/en/wb_sim/ds_timeline_tab_data.html
- [S20] Messages Window: https://ansyshelp.ansys.com/public/Views/Secured/corp/v261/en/wb_sim/ds_mess_win.html
- [S21] Perform Solution: https://ansyshelp.ansys.com/public/Views/Secured/corp/v261/en/wb_sim/ds_Solve_step.html
- [S22] Mecway manual, Version 34: https://mecway.com/manual.pdf (linked from https://mecway.com/product.html)
- [S23a] SALOME desktop (9.16.0): https://docs.salome-platform.org/latest/gui/GUI/salome_desktop.html
- [S23b] Introduction to GUI: https://docs.salome-platform.org/latest/gui/GUI/introduction_to_gui.html
- [S23c] Using Object Browser: https://docs.salome-platform.org/latest/gui/GUI/using_object_browser.html
- [S24] code_aster documentation, v3.02.326 Modelling A (AsterStudy steps): https://docaster-en-codeaster-doc-4113bb35b8213385c636b94f1536856d7f617.gitlab.io/manuals/man_v/v3/v3.02.326/Mod_lisation_A.html
- [S25] ElmerGUI Manual (CSC, built 2026-01-20): https://www.nic.funet.fi/pub/sci/physics/elmer/doc/ElmerGUIManual.pdf (Fig. 4.2, s4.2, s4.6, s7, s9)
- [S26] CMB ModelBuilder, Getting Started: https://cmb.readthedocs.io/en/latest/ModelBuilder/Getting%20Started.html
- [S27] Its screenshot: https://cmb.readthedocs.io/en/latest/_images/WindowLayout.png
- [S28] SMTK ParaView panels: https://smtk.readthedocs.io/en/latest/userguide/extension/paraview/panels.html
- [S29] tomviz docs: https://tomviz.readthedocs.io/ ; [S29b] https://tomviz.readthedocs.io/en/latest/pipeline_management.html
- [S30] https://github.com/OpenChemistry/tomviz/blob/master/CMakeLists.txt
- [S31] https://github.com/OpenChemistry/tomviz/blob/master/tomviz/MainWindow.ui
- [S32] https://github.com/OpenChemistry/tomviz/blob/master/tomviz/CentralWidget.ui
- [S33] ParaView User's Guide, Introduction: https://docs.paraview.org/en/latest/UsersGuide/introduction.html
- [S34] Understanding Data (Information panel): https://docs.paraview.org/en/latest/UsersGuide/understandingData.html
- [S35] https://gitlab.kitware.com/paraview/paraview/-/blob/master/Clients/ParaView/ParaViewMainWindow.ui and `ParaViewMainWindow.cxx` (lines 100 to 200)
- [S36] Displaying Data, Multiple views: https://docs.paraview.org/en/latest/UsersGuide/displayingData.html
- [S37] CMB, https://gitlab.kitware.com/cmb/cmb commit c2cc2c3a2c69 (2026-11-05): [S37a] `modelbuilder/mbLayoutSpec.cxx`; [S37b] `cmb/cmbMainWindow.cxx` (lines 232 to 254, 395 to 541)
- [S38] SMTK, https://gitlab.kitware.com/cmb/smtk commit 6ada5c69e8c4 (2026-11-09): [S38a] `doc/userguide/attribute/concepts.rst`; [S38b] `doc/userguide/attribute/file-syntax.rst` (Category, Analysis, AssociationsDef, Views sections); [S38c] `data/attribute/attribute_collection/elasticity.sbt`; [S38d] `data/attribute/attribute_collection/HydraTemplateV1.sbt` (lines 1460 to 1471); [S38e] `doc/userguide/extension/paraview/panels.rst`; [S38f] `smtk/extension/paraview/appcomponents/pqSMTKExportSimulationBehavior.cxx`; [S38g] `doc/userguide/simulation/index.rst`; [S38h] `doc/userguide/task/concepts.rst`; [S38i] `data/attribute/attribute_collection/unitsExample.sbt`
- [S39] aeva, https://gitlab.kitware.com/aeva/aeva commit c978fab2bb5d (2026-11-07): `doc/userguide/aeva-cmb.rst` (Operations panel)
- [S40] SMTK's Simulation System: https://smtk.readthedocs.io/en/latest/userguide/simulation/index.html
