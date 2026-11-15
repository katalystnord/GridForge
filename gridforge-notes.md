# GridForge: working notes

A future open-source Qt/VTK GUI for simulation: CalculiX (structures)
first, OpenFOAM (fluids) later. Named by David on 2026-11-13. Not started.

The name: fluid (OpenFOAM) and stress (CalculiX), one word for each
solver; and, as David noted, being fluid under stress, "in the zone".
**Name not final (David, 2026-11-13): GridForge or FlowTau, TauFlow, to decide
later.** The file keeps the GridForge name meanwhile.

- **GridForge**: says it plainly; free on the web and GitHub; weak as a
  trademark (an ordinary phrase).
- **FlowTau**: tau is the shear-stress symbol in fluids and solids alike;
  a coined word, stronger as a trademark; one unrelated GitHub repository
  (flowtaumate); echoes DLR's TAU, a well-known CFD solver for compressible
  flow, which CFD people may associate with it.

First named SolidFlow the same day, dropped for being too close to
Dassault's SOLIDWORKS Flow Simulation; ForceFlow was also considered (an
HR product of that name exists, and Karamba has "ForceFlow lines").

## Why it would exist (2026-11-13)

From the TenSense Phase 4 discussion: the VTK ecosystem is strong at
post-processing (ParaView) but has no modern, pleasant standalone
pre-processor for nonlinear solid mechanics on Linux. What was looked at:

- **Range FEA 1.2.2** (installed, GPL-3, Tomas Soltys): pleasant GUI, but
  its stress analysis is linear elastic only (material properties: modulus,
  Poisson's ratio, density, thermal expansion; no yield or plasticity),
  binary material files of its own, no results export found.
- **PrePoMax** (GPL-3, C#/.NET Framework 4.8/WinForms, VTK 5.8 via
  ActiViz): the good CalculiX GUI, Windows only; not tried under Wine.
- **FreeCAD FEM**: works on Linux for CalculiX and Elmer, heavy.
- **Salome-Meca** (code_aster, VTK viewers, ParaView built in): complete but
  heavy; not checked here.
- **Elmer**: native .vtu output and multiphysics, but its UMAT library has
  elastic models only (checked in fem/src/modules/UMATLib.F90): no
  plasticity, so TenSense's hardening curve has nowhere to go.
- **CalculiX**: plasticity built in (*PLASTIC, isotropic or kinematic);
  TenSense already writes its block, and its test solves FreeCAD's
  nonlinear example with it. Results come out as .frd (not VTK).

## Settled so far

- **Position (David, 2026-11-13): directly between FreeCAD and ParaView.**
  Geometry comes from FreeCAD as files (STEP or BREP, named faces if
  possible); GridForge does no CAD modelling. GridForge does meshing,
  physics set-up (materials, TenSense's measured ones included; loads,
  supports, steps), runs the solver, and gives a quick look at results.
  Results go to ParaView as .vtu/.pvd; detailed post-processing is not
  rebuilt. The measured-against-predicted comparison (SurView field,
  TenSense force) lives in GridForge, its result handed to ParaView like
  any other. Files at both ends, never code.

- **Solver-neutral core**: geometry, mesh, named selections, VTK results,
  jobs; each solver a back end writing its own input files.
- **CalculiX first** (matches TenSense's materials and the DIC comparison
  with SurView); **OpenFOAM** as the CFD back end when there is a case for
  it; Elmer a candidate for heat and coupled problems.
- The measured-against-predicted comparison (SurView's field, TenSense's
  force, CalculiX's prediction) is the part nobody else provides.
- Independent kit, like SurView and TenSense: files between them, never
  code.

## Open

- **Name check** (for the working name FluidStress, before the app was
  named GridForge on 2026-11-15): web search 2026-11-13 found no product named
  FluidStress and no GitHub repository; "fluid stress" is an ordinary
  term (Goma's output option, Wolfram's FluidViscousStress), so the name
  is free but weak as a trademark. A formal check (TMview, PRV, EUIPO)
  before relying on it.
- Scope and order against TenSense Phase 6 (the measured-against-predicted
  loop).
