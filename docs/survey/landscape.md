# Landscape: open pre-processors and GUIs for CalculiX and OpenFOAM (Linux focus)

Survey date: 2026-11-14. Method: project pages, GitHub API (licence files, last commit, releases),
manuals, forum threads. Every fact below has its source URL. "Not checked" means I could not reach
or confirm the source. Access limits in this pass: reddit.com is blocked for the research tools (so
r/CFD and r/fea were **not checked**); cfd-online.com returns HTTP 403 to page fetches (CFD Online
items below come from search-result snippets only and are marked so); wiki.freecad.org and
forum.freecad.org are behind a bot wall (FreeCAD facts come from the FreeCAD source tree and its
GitHub issues instead).

---

## 1. Tool by tool

### CalculiX side

**CalculiX itself (ccx 2.23 / cgx 2.23)**
- Licence: GPL-2.0 (https://www.dhondt.de/ links GPL v2; GitHub repo https://github.com/Dhondtguido/CalculiX shows GPL-2.0). Current version 2.23 for both ccx and cgx per https://www.dhondt.de/. Repo last commit 2026-11-10 (GitHub API).
- Output formats: ccx writes .frd (and .dat). The ccx manual source (`doc/CalculiX.tex` in the repo above) contains no mention of VTK, Exodus or ParaView output; OUTPUT=2D/3D only controls how shells and beams are expanded in the .frd. So every route to ParaView goes through a converter.
- Fact that matters for a converter: 1D/2D elements are written "in their expanded three-dimensional form" by default (OUTPUT=3D) (CalculiX.tex, *NODE FILE section).

**CalculiX GraphiX (cgx)**
- Licence GPL-2.0, Linux and Windows binaries (https://www.dhondt.de/).
- Toolkit: OpenGL with GLUT for windows and events (cgx manual: https://web.mit.edu/calculix_v2.7/CalculiX/cgx_2.7/doc/cgx/node3.html); Debian patches it to freeglut (https://sources.debian.org/src/calculix-cgx/2.21%2Bdfsg-2/debian/patches/0002-Using-freeglut.patch).
- Workflow: command-driven (.fbd command files), its own geometry and mapped meshing. Native on Linux, light on large models.
- Users: "CGX => Native, complex to start with it." (SergioP, 2021-06-18, https://calculix.discourse.group/t/pre-post-processing-open-software/630).
- Results to ParaView: no; cgx is itself the viewer.

**PrePoMax**
- Licence GPL-3.0 ("licensed under the GNU General Public License v3", https://github.com/PrePoMax/PrePoMax; also https://prepomax.fs.um.si/downloads/).
- Latest: v3.0.0 dated 2 November 2026 on https://prepomax.fs.um.si/downloads/. Official GitHub repo created 2026-11-04, last commit 2026-11-05 (GitHub API); older mirror on GitLab (MatejB/PrePoMax).
- Platform: Windows. Requires .NET Framework 4.8 (downloads page). C#; 3D via VTK through ActiViz 5.8.0 (repo README). Linux is not mentioned anywhere in the README.
- Linux: only via Wine or a VM. A Wine recipe exists (https://github.com/jcugnoni/PrePoMax_on_Linux_with_wine, tested with v2.3.5 dev and Wine staging 10.12; table text shows square symbols). Forum, 2026-03: "from prepomax v2.5.0 is not working"; "Intel graphics causes the program to freeze during a surface selection." (https://prepomax.discourse.group/t/prepomax-on-linux-with-wine/3360).
- Why no native port: the developer said ActiViz moved from open source to a licensed product, "So I think this will be a challenge to port to Linux." (Matej, 2022-08-29, https://prepomax.discourse.group/t/building-on-mono/106).
- Third-party port: "prepomac" replaces WinForms with Avalonia and runs .NET 10; Linux "experimental", README: "usable, still being tested" (https://github.com/moritzschuettler/prepomac, GPL-3.0, created 2026-10-18).
- Workflow model (manual v2.4.0, https://prepomax.fs.um.si/wp-content/uploads/2025/10/PrePoMax-v2.4.0-manual.pdf): three model-tree tabs, "Geometry", "FE Model", "Results". FE Model menus: Property/materials library, Interaction, Distribution, Amplitude, Initial Condition, Step (with per-step BCs, loads, field and history output). Step types: static, frequency, steady-state dynamics, modal dynamics, implicit and explicit dynamic, buckling, heat transfer, coupled and uncoupled thermo-mechanical, slip wear. Strongest idea: a command **history** that can be "regenerated", including "Regenerate History Using Other Files" (swap in a changed geometry without redefining the model) and "Regenerate History With Remeshing". Command-line regeneration for automation (-r, .pmx/.pmh).
- Import: STEP, IGES, STL, .unv, .vol, .inp, .mesh; opens .frd and OpenFOAM .foam. Export: STEP, BREP, STL, CalculiX/Abaqus .inp, .msh (experimental), deformed mesh .inp. History outputs to CSV. **No VTK/VTU export** in the manual's export list.
- On VTU export the developer wrote: "That is why this feature is very low on the to-do list." (2022-06-14, https://prepomax.discourse.group/t/export-to-vtu-format-for-paraview/247).
- Does well: complete CalculiX coverage, multistep, contact, material library, regeneration. Lacks: Linux, ParaView hand-off.

**CalculiX Launcher (bConverged / calculixforwin)**
- SourceForge project licence "Creative Commons Attribution ShareAlike License V2.0", last update 2023-05-29, listed for Windows/Cygwin (https://sourceforge.net/projects/calculixforwin/).
- A 2016 announcement mentions a Linux version with a boundary-condition wizard, "Almost 80%" of CalculiX features in the GUI (https://imechanica.egr.uh.edu/node/20489). Toolkit: not checked. Current Linux build: not checked (newer versions live on a blog, not on SourceForge). Effectively dormant.

**CAE (CalculiX Advanced Environment, Ihor Mirzov)**
- GPL-3.0 (LICENSE file), Python 3 + PyQt5 keyword editor driven by an editable XML keyword list; runs cgx alongside and sends it commands; Linux and Windows (https://github.com/calculix/cae). Last push 2024-10-09 (GitHub API). It is an .inp keyword editor, not a model-tree pre-processor.

**ccx2paraview (converter, not a GUI)**
- GPL-3.0, Python, last push 2026-09-21 (https://github.com/calculix/ccx2paraview). Converts **ASCII** .frd to .vtk/.vtu, one file per output interval ("If you have 300 time steps in the FRD, there will be 300 Paraview files").
- Pain: 3 GB .frd took "28.5 minutes to convert" vs "CalMED took 5 minuts" (https://calculix.discourse.group/t/converter-ccx2paraview-for-large-frd-files/1259, 2022-09).

**FreeCAD FEM workbench**
- FreeCAD LGPL-2.1 (GitHub API on https://github.com/FreeCAD/FreeCAD); latest stable 1.1.4 (2026-09-28), weekly builds to 2026-11-11.
- Toolkit: Qt + Coin3D, VTK for the result pipelines (version string in issue #11639 shows "Qt 5.15.8, Coin 4.0.0, Vtk 9.2.6"). Linux native.
- Solvers in tree: CalculiX, Elmer, Z88, Mystran, FEniCS (`src/Mod/Fem/femsolver`). Meshers: Gmsh, Netgen (`femmesh/gmshtools.py`, `netgentools.py`).
- Workflow model: an Analysis container object holding solver, mesh, materials, constraints and results; geometry references by face/edge of the parametric shape.
- Results: imports .frd (`importCcxFrdResults`) and VTK (`importVTKResults`), result pipelines are VTK, can export to VTK (Init.py registrations).
- Open issues that matter: single step only, "CalculiX analysis ... can have only one step" (#11639, open, https://github.com/FreeCAD/FreeCAD/issues/11639); "FreeCAD just freezes providing no feedback until the results" (#12706, open, https://github.com/FreeCAD/FreeCAD/issues/12706); dynamic analyses missing (#11647, open); two confusing result objects (#10821, open); pipeline legends locked to SI m/Pa (#6922, open, https://github.com/FreeCAD/FreeCAD/issues/6922).

**Salome / Salome-Meca**
- SALOME 9.16.0, release notes dated June 2026; LGPL ("distributed under terms of the GNU Lesser General Public License"); prerequisites Qt 5.15.15/5.15.17, PyQt 5.15.3, **ParaView 6.0.0** bundled as PARAVIS (https://www.salome-platform.org/wp-content/uploads/2026/07/SALOME_9_16_0_Release_Notes.pdf). Downloads for Debian 9 to 13, Ubuntu 18 to 24, Fedora, CentOS, Windows (https://www.salome-platform.org/?page_id=2430).
- Workflow: modules (SHAPER/GEOM geometry, SMESH meshing, PARAVIS results) in one study tree. Geared to code_aster (Salome-Meca) and code_saturne, not CalculiX: no CalculiX case setup module found (not checked exhaustively).
- Salome-Meca: a CSMA 2026 paper says the 2025 platform was released early 2026; earlier builds ship as an Apptainer image (search result pointing to https://discourse.salome-platform.org/c/install-build/5). I could not open the code-aster.org download page (404), so current version: **not checked**.
- Users: Salome-Meca "is hard to use because you have to set a lot of things." (Calc_em, 2022-01-26, https://calculix.discourse.group/t/where-to-start-with-open-source-fea/969).

**Mecway**
- Proprietary. US$500 perpetual with a year of updates, licensed version 34 (https://mecway.com/buy.html). Windows ("for Windows", https://mecway.com/). Bundles CalculiX for nonlinear, OpenRadioss for explicit. Free version limited to 1000 nodes (per Wikipedia via search; Mecway's own page not confirmed). Wine reports on the Mecway forum are old and mixed (not re-checked).

**CAELinux**
- Last release CAELinux 2020, Xubuntu 18.04 base, with Salome-Meca 2019, OpenFOAM v7, CalculiX (https://lwn.net/Articles/829204, https://www.debugpoint.com/caelinux-2020-release/). caelinux.com returned HTTP 503 during this survey. No later release found: likely dormant (not confirmed with the author).

**Gmsh (as pre-processor)**
- 4.15.2, 2026-03-24; "GNU General Public License (GPL) ... version 2 or later, with an exception to allow for easier linking"; FLTK + OpenGL GUI (https://gmsh.info/).
- Mesh export includes inp, unv, vtk, med, cgns, su2, bdf (Mesh.Format option list in https://gmsh.info/doc/texinfo/gmsh.html). No physics set-up for CalculiX: you get a mesh and physical groups, then hand-write the deck.

**Range FEA**
- GPL-3.0, C++/Qt 6 (needs "Qt version 6.12 or newer"), own solver (`fea-solver`), TetGen meshing; Linux DEB/RPM/tar.gz, Windows, macOS; v1.4.0 released 2026-11-11 (https://github.com/Range-Software/range-fea, GitHub API). Not a CalculiX or OpenFOAM front end, but proof that a single-person native Qt 6 FEA desktop app with Linux packages is viable. Export formats: not stated in README (not checked further).

**ElmerGUI (pattern)**
- GPL-2 (ElmerGUI is under GPL while the solver library is LGPL, per `license_texts/ElmerLicensePolicy.md` in https://github.com/ElmerCSC/elmerfem). Qt 5 or Qt 6 (CMakeLists), optional VTK post and OpenCASCADE import. Release 26.2.1 (2026-04-29).
- Pattern: each solver's GUI panel is generated from XML definition files (`ElmerGUI/Application/edf/*.xml`, e.g. `linearelasticity.xml`, `heatequation.xml`), so new physics needs no C++.

**code_saturne GUI (pattern)**
- GPL-2.0 (GitHub API on https://github.com/code-saturne/code_saturne), v9.2.0 tag. Python GUI with both `qt5` and `qt6` back ends (`python/code_saturne/gui/base`).
- Pattern: the GUI reads and writes one XML case file "according to a specific code_saturne schema" which the solver scripts interpret; fixed case layout DATA/SRC/RESU, and `code_saturne create` does the same as the GUI's new-case action (https://code-saturne.org/doc/code_saturne-9.2/cs_ug_case_structure.html, search snippet). GUI and command line are interchangeable over the same file.

### OpenFOAM side

**FreeCAD CfdOF workbench**
- LGPL-3.0 (LICENSE file), last commit 2026-11-09, tag v1.37.3 (https://github.com/jaheyns/CfdOF). Python inside FreeCAD (needs FreeCAD 1.0+).
- Supports OpenFOAM Foundation 9 to 13 and OpenCFD 2006 to 2512; meshing with cfMesh, snappyHexMesh, Gmsh; RANS/LES/DES, interFoam, buoyant, HiSA; Windows and Linux; post-processing "using Paraview" (README).
- Workflow: same FreeCAD analysis-container idea (CfdAnalysis with physics, fluid properties, BC objects, mesh, solver).
- Pain (GitHub issues): "CfdOF not responding" (#141, open); default output dir "/tmp/case" (#148); cfMesh install "taking a really long time" (#214, open); third-party mesh import wanted (#89, open); MRF request (#189, open).

**HELYX-OS (ENGYS)**
- **Deprecated.** README: "The HELYX-OS project has been deprecated" (https://github.com/ENGYS/HELYX-OS). GPL-3.0-or-later (COPYING). Java + VTK, Linux binary. Targets OpenFOAM 4.1 and v1606+. Last release v2.4.0 (2016-11-22); last commit 2025-02-05 (GitHub API).
- Open issues are startup failures: "Helyx-OS cannot be started" (#49), "Not getting 3D graphics Window" (#47).

**SimFlow**
- Proprietary. Free version: "the mesh size is limited to 200,000 nodes", "1 CPU only", "cases cannot be imported or exported"; Windows and Linux; annual subscription or perpetual; SimFlow 5.1 recommends OpenFOAM 2512 (https://help.sim-flow.com/faq). Note: simflow.com now redirects to an unrelated simflow.ai site; the product lives at sim-flow.com.

**Visual-CFD (ESI, now Keysight)**
- Proprietary. Keysight's download page lists only "VE 18.0 for OpenFOAM v2112", released 2022-11-15, Windows 64-bit, "No previous versions available" (https://www.keysight.com/us/en/lib/software-detail/des/visual-environment-openfoam.html). The old ESI product URL redirects to a generic Keysight CAE page. Current Linux availability: not checked; looks stalled.

**DEXCS for OpenFOAM**
- Japanese bootable Linux distribution bundling OpenFOAM, ParaView, cfMesh, TreeFoam, FreeCAD and a FreeCAD-macro launcher (DEXCS2019 slides, https://www.slideshare.net/slideshow/about-dexcs2019-for-openfoam/181934914). DEXCS2020 bundled HELYX-OS (DEXCS2021 slides, https://www.slideshare.net/slideshow/about-dexcs2021-for-openfoam/250412510). SlideShare listing shows DEXCS2025 and DEXCS2026 install decks (search snippet). Official site dexcs.gifu-nct.ac.jp did not resolve. Licence of the macros, current contents: **not checked**.

**SplashFOAM (CFD Dose)**
- LGPL-3.0, Python + PySide6, v0.2 (2024-12-22), last commit 2025-03-16 (https://github.com/cfddose/Splash). Claims it handles "all OpenFOAM versions, whether ESI or Foundation". Results: "launch ParaView directly". Small project, 115 stars.

**Venturial (FOSSEE)**
- GPL-3.0, a Blender add-on that builds blockMesh/snappyHexMesh dictionaries and case templates; v0.2.0 2025-08-03; recommends Ubuntu 20.04/22.04 (https://github.com/FOSSEE/venturial).

**Stokes (stokes-cfd)**
- PolyForm Noncommercial 1.0.0 (LICENSE), so **not open source**; one maintainer; QtWebEngine host with React/three.js front end; OpenFOAM Foundation 14 on Linux and Windows; STEP import, click faces to set patches, snappyHexMesh or cfMesh; created 2026-11-09 (https://github.com/dhanninacs/stokes-cfd). Shows current demand for exactly this workflow.

**Other GitHub OpenFOAM GUIs with some use (GitHub search "openfoam gui", sorted by stars)**
- hexBlocker: Qt + VTK blockMeshDict editor, last push 2016 (https://github.com/nicolasedh/hexBlocker). Dead but the right stack.
- OpenFOAM Studio (Pat2704/openfoam-gui): MIT, Windows + WSL2, AI copilots, active 2026-11 (https://github.com/Pat2704/openfoam-gui).
- Magnusim: GPL-3.0-or-later, Windows web app over WSL, OpenFOAM v2606 (https://github.com/Lilmill2000/Magnusim).
- OpenFOAMGUI (byChen47): Qt 6.10.2 case editor, MinGW build (Windows), licence not stated (https://openfoamgui.readthedocs.io, https://github.com/byChen47/OpenFOAMGUI).
- CFDTool / FEATool (precise-simulation): MATLAB-based, licence NOASSERTION on GitHub; terms not checked.

**PyFoam / swak4Foam / cfMesh**
- PyFoam 2026.6 (2026-06-30), GPL v2 or later, CLI utilities for running, log analysis, residual plots, case editing "strictly from the outside" (https://pypi.org/project/PyFoam/). Not a GUI, but its log parser and plot runner define what "monitoring" means for OpenFOAM users.
- swak4Foam: expression libraries; no clear support for recent OpenCFD versions (OpenFOAMWiki warns newer versions "might not be supported", https://mobile.openfoamwiki.net/index.php/Contrib/swak4Foam); not a GUI.
- cfMesh GUI: none exists as a standalone tool. Creative Fields' cfSuite (2014) is historical (https://www.cfd-online.com/Forums/openfoam-news-announcements-other/136908-creative-fields-releases-cfsuite-1-0-cfmesh-1-0-a.html, title only). CfdOF is the de facto cfMesh GUI.

**SU2 GUI (pattern)**
- su2gui: GPL-3.0, Python, trame + VTK (browser UI), 1.0.2 on PyPI (2025-09-10), last commit 2026-02-16 (https://github.com/su2code/su2gui). Config is validated against a JSON schema (`JsonSchema.json`, `su2_validation_schema.json`), shown as JSON and converted back to an SU2 config at run time (https://su2code.github.io/su2gui/configurations).

---

## 2. Pain points (what users complain about or wish for)

CalculiX:
- No Linux PrePoMax: "Shame I cant run it in Linux." (carlomontec, 2021-06-30, https://calculix.discourse.group/t/pre-post-processing-open-software/630). Wine breaks across versions (thread 3360 above).
- Keyword coverage gaps force hand-editing: "Some cards will not load, so maybe you will miss something important." (SergioP, 2021-06-18, same thread).
- ParaView cannot read .frd: "Please add native opening of output files (.frd)" (2021-08-12); converters "all lacks of some variables or are not easy to use" (2021-12-10); Kitware noted "166 like on uservoice" and replied with a Python conversion tutorial, not a reader (2023-06-01) (https://discourse.paraview.org/t/calculix-format-native-support-frd/7807).
- Named sets lost on the way to ParaView: "I cannot access named boundary surfaces" and "sets are not stored in the .frd files" (2026-02-09, https://calculix.discourse.group/t/salome-ccx-paraview-workflow-issue/3775).
- Too many files for transient/nonlinear output: workstation "suffer a lot reading the individual increments/frames" (2022-09-28, ccx2paraview thread 1259).
- ParaView + CalculiX for newcomers: "couldn't work out how to get it to read the Calculix outputs." (PRhodes, 2022-02-21, thread 969).
- FreeCAD FEM: single step (#11639), blocks the UI while ccx runs (#12706), SI-only legends (#6922), confusing result objects (#10821).
- Salome-Meca too heavy for the task (thread 969 quote above).

OpenFOAM:
- Hand-editing dictionaries (CFD Online, search snippet only, page 403): "I spent countless hours manually editing OpenFOAM dictionaries" (https://www.cfd-online.com/Forums/openfoam/262356-foampilot-open-source-desktop-gui-openfoam-electron-fastapi-docker-seeki.html).
- Utility chains: "setting up long chains of commands can be rather tedious" (search snippet, https://www.cfd-online.com/Forums/openfoam/179014-discussion-should-we-create-gui-interface-utilities-working-prototype.html).
- Free GUIs vanish or lock versions: HELYX-OS deprecated and pinned to OpenFOAM 4.1/v1606+; Visual-CFD download stuck at v2112 (2022); SimFlow free tier 200k nodes, 1 CPU.
- Install friction: CfdOF #214 (cfMesh install), #301 "Docker permission issue when meshing"; HELYX-OS #49/#50 cannot start.
- Fork split: tools must state which of Foundation (13, 14) or OpenCFD (2512, 2606) they support; CfdOF supports both ranges, Stokes only Foundation 14, SimFlow only openfoam.com.

Reddit (r/CFD, r/fea): **not checked** (blocked for the research tools).

---

## 3. Summary table

| Tool | Licence (checked) | Linux | Toolkit | Last release / commit | Solver | Workflow model | Results to ParaView |
|---|---|---|---|---|---|---|---|
| cgx | GPL-2.0 | native | OpenGL + GLUT | 2.23 | CalculiX | command files (.fbd) | none (own viewer) |
| PrePoMax | GPL-3.0 | Wine/VM only; Avalonia fork experimental | C# WinForms (inferred), ActiViz/VTK | v3.0.0, 2026-11-02 | CalculiX | 3-tab tree, steps, regenerable history | none built in; developer: low priority |
| CalculiX Launcher | CC BY-SA 2.0 (SF page) | old Linux build | not checked | SF 2023-05-29 | CalculiX | wizard | not checked |
| CAE (Mirzov) | GPL-3.0 | native | PyQt5 + cgx | push 2024-10-09 | CalculiX | .inp keyword tree | via ccx2paraview |
| FreeCAD FEM | LGPL-2.1 | native | Qt, Coin3D, VTK | 1.1.4, 2026-09-28 | ccx, Elmer, Z88, Mystran | Analysis container | VTK export |
| CfdOF | LGPL-3.0 | native (in FreeCAD) | Python in FreeCAD | commit 2026-11-09 | OpenFOAM (both forks) | CfdAnalysis container | writes case, opens ParaView |
| Salome 9.16 | LGPL-2.1 | native packages | Qt 5.15, PyQt 5, ParaView 6.0 | June 2026 | code_aster, code_saturne | study tree, modules | built-in PARAVIS |
| Salome-Meca | LGPL (Salome) | Apptainer | as Salome | 2025 edition early 2026 (paper) | code_aster | as Salome | PARAVIS |
| HELYX-OS | GPL-3.0+ | native (Java) | Java + VTK | v2.4.0 2016; deprecated | OpenFOAM 4.1 / v1606 | case tree | not checked |
| SimFlow | proprietary | yes | not checked | 5.1 | OpenFOAM (.com) | wizard | built in + ParaView (not checked) |
| Visual-CFD | proprietary | page lists Windows only | not checked | VE 18.0, 2022-11-15 | OpenFOAM v2112 | not checked | not checked |
| DEXCS | not checked | is a Linux distro | FreeCAD macros, TreeFoam | DEXCS2026 decks exist | OpenFOAM | not checked | ParaView bundled |
| Mecway | proprietary, US$500 | Wine, mixed reports | not checked | v34 | built-in + CalculiX | tree | not checked |
| CAELinux | distro | is a Linux distro | n/a | 2020 | many | n/a | n/a |
| Gmsh | GPL-2+ with linking exception | native | FLTK + OpenGL | 4.15.2, 2026-03-24 | none (mesh only) | .geo script | exports .vtk mesh |
| Range FEA | GPL-3.0 | native DEB/RPM | C++ Qt 6 | v1.4.0, 2026-11-11 | own | not checked | not checked |
| ElmerGUI | GPL-2 | native | Qt 5/6, VTK opt. | 26.2.1, 2026-04-29 | Elmer | XML-defined solver panels | Elmer writes VTU (not checked here) |
| code_saturne GUI | GPL-2.0 | native | PyQt 5/6 | v9.2.0 | code_saturne | one XML case file | not checked |
| su2gui | GPL-3.0 | browser (trame) | Python, VTK | 1.0.2, 2025-09-10 | SU2 | JSON-schema config | VTK in app |
| SplashFOAM | LGPL-3.0 | native | PySide6 | v0.2, 2024-12; commit 2025-03 | OpenFOAM (both) | modular panels | launches ParaView |
| Venturial | GPL-3.0 | Blender add-on | Blender/Python | v0.2.0, 2025-08-03 | OpenFOAM | Blender scene | not checked |
| Stokes | PolyForm NC (not OSS) | native | QtWebEngine + React | 2026-11 | OpenFOAM 14 | click-faces workbench | own viewer |
| PyFoam | GPL-2+ | CLI | Python | 2026.6 | OpenFOAM | CLI utilities | n/a |
| ccx2paraview | GPL-3.0 | yes | Python | push 2026-09-21 | CalculiX | converter | one file per increment |

---

## 4. Gaps no tool fills on Linux (each tied to evidence above)

1. **A native, open, maintained CalculiX pre-processor with PrePoMax-class workflow.** PrePoMax is Windows-only and its port is blocked on ActiViz; Wine works by version and GPU luck; cgx is native but command-driven; CAE is a keyword editor; FreeCAD FEM lacks multistep and dynamics.
2. **Multistep nonlinear set-up on Linux.** Only PrePoMax (Windows) does steps properly; FreeCAD #11639 open since 2023.
3. **A clean CalculiX to ParaView hand-off.** ccx writes only .frd; ParaView has no reader (Kitware thread); ccx2paraview is ASCII-only, slow on large files, splits increments into many files, drops named sets; PrePoMax will not add VTU export. Nobody writes a single time-series file (.pvd or VTKHDF) with node/element/surface sets carried along.
4. **Non-blocking solver runs with live feedback.** FreeCAD #12706 (open); CfdOF #141 "not responding".
5. **A standalone, open OpenFOAM set-up GUI on Linux that is alive.** HELYX-OS deprecated (2016 release), Visual-CFD stalled on v2112 and Windows download, SimFlow proprietary and capped, Stokes non-commercial, SplashFOAM small and quiet since 2025-03, CfdOF tied to FreeCAD.
6. **Geometry-change robustness without a CAD kernel in the loop.** Only PrePoMax has "Regenerate History Using Other Files"; nothing on Linux re-applies loads and BCs to a re-exported STEP.
7. **One project format readable by both GUI and scripts for CalculiX.** code_saturne (XML case) and su2gui (JSON schema) show the pattern; no CalculiX tool exposes its model as a documented, diffable text file other than the solver deck itself.

---

## 5. Features, ranked: SHOULD have (core) and COULD have (later)

SHOULD (each fills a gap above):
1. **Native Linux packaging (deb/rpm/AppImage), Qt 6 + VTK.** Evidence: PrePoMax Linux complaints (threads 630, 3360, building-on-mono 106); Range FEA shows the stack ships fine on Linux.
2. **Step-based model tree: Geometry, Mesh, Model (materials, sets, interactions, amplitudes), Steps (BCs, loads, outputs per step), Results.** Evidence: PrePoMax 3-tab tree and step types (manual v2.4.0); FreeCAD #11639 single-step limit.
3. **First-class nonlinear solids: plasticity, contact, NLGEOM, increment control.** Evidence: Katalyst Nord's stated scope; FreeCAD has `material_mechanicalnonlinear` but no multistep; PrePoMax covers it on Windows only.
4. **Excellent .frd to VTK writer: binary and ASCII .frd, one .pvd or VTKHDF time series, all variables, node/element/surface sets as blocks or cell fields, von Mises/principal derived.** Evidence: ParaView thread 7807, ccx2paraview thread 1259, Salome-CCX-ParaView thread 3775, PrePoMax 247. Note: ccx2paraview is GPL-3.0, so an LGPL-2.1-or-later tool must write its own reader rather than copy that code.
5. **Solver as a monitored child process: non-blocking, live log, convergence/increment plot (ccx .sta/.cvg), cancel, queue.** Evidence: FreeCAD #12706, CfdOF #141; PyFoam's plot runner as the OpenFOAM precedent.
6. **Raw-keyword passthrough and a readable generated deck.** Let users add any CalculiX card per step and keep it on regeneration; show the .inp. Evidence: "Some cards will not load" (thread 630); *DYNAMIC missing in PrePoMax in 2021 (same thread).
7. **Geometry swap with persistent assignments.** Re-import a changed STEP/BREP and re-map BCs by face matching, like PrePoMax's "Regenerate History Using Other Files". Evidence: PrePoMax manual; the FreeCAD-in, files-only architecture makes this the main daily friction.
8. **Units chosen per project and shown everywhere (mm, t, s, MPa).** Evidence: FreeCAD #6922; PrePoMax unit-system choice at model creation.
9. **Text project file with a schema (JSON or XML), so GUI and scripts drive the same model.** Evidence: code_saturne XML case, su2gui JSON schema, ElmerGUI edf XML; PrePoMax command-line regeneration shows the demand for automation.
10. **Meshing via Gmsh (and Netgen) as external processes, with named physical groups kept as CalculiX sets.** Evidence: Gmsh exports .inp; PrePoMax and FreeCAD both use Gmsh/Netgen; sets loss in thread 3775.

COULD (later, ranked):
11. **OpenFOAM back end on the same model tree** (patches, physics, snappyHexMesh/cfMesh dictionaries, both forks declared explicitly). Evidence: HELYX-OS deprecated, Visual-CFD stalled, SimFlow capped; CfdOF shows both forks can be supported.
12. **Schema-driven physics panels** so new keywords or OpenFOAM solvers are data, not code. Evidence: ElmerGUI edf XML, CAE's editable XML keyword list, su2gui JSON schema.
13. **Results quick look in-app** (contours, deformed shape, probe, history plot, CSV export), deferring real post to ParaView. Evidence: PrePoMax history CSV export; complaint that ParaView is hard for CalculiX newcomers (thread 969).
14. **Material library with exchange format.** Evidence: PrePoMax "Exchanging material data between users" (manual section 5.2.5).
15. **Remote or container solver runs** (SSH, Apptainer). Evidence: CfdOF #70 (133 comments) on remote meshing and simulation; Salome-Meca ships as Apptainer.
16. **Read OpenFOAM results (.foam) for the quick look.** Evidence: PrePoMax already opens .foam files.
17. **Scriptable API / CLI regeneration** (batch parameter studies). Evidence: PrePoMax -r regeneration and .pmp parameters; Stokes "Every action is also an API call".

What the tool should NOT try to be (evidence-based): a CAD modeller (FreeCAD does it; geometry comes in as STEP/BREP) or a post-processor (ParaView 6.0 already ships even inside Salome). The gap is the middle: Linux-native, step-based, nonlinear-capable set-up with a lossless hand-off on both sides.
