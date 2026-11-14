# FluidStress (working name)

Open simulation set-up between FreeCAD and ParaView: mesh, set up, run
and hand over CalculiX analyses, and later OpenFOAM® cases, natively on
Linux. Geometry comes in from FreeCAD as files; results go out to
ParaView as files. Nonlinear solids first: plasticity, with materials
measured by TenForge.

**Status: skeleton (2026-11-14).** Nothing to use yet. What it should be,
and why, is in [docs/survey/](docs/survey/README.md); the plan in
[ROADMAP.md](ROADMAP.md).

## Build

Qt 6, VTK 9 and CMake (Ubuntu 26.04 packages):

```bash
tools/run-tests.sh
```

## Licence

LGPL-2.1-or-later. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

CalculiX, Gmsh and OpenFOAM are separate programs under their own
licences (GPL); FluidStress runs them and reads and writes their files,
and does not include their code.

---

This offering is not approved or endorsed by OpenCFD Limited, producer
and distributor of the OpenFOAM software via www.openfoam.com, and owner
of the OPENFOAM® and OpenCFD® trade marks.

OPENFOAM® is a registered trade mark of OpenCFD Limited, producer and
distributor of the OpenFOAM software via www.openfoam.com.
