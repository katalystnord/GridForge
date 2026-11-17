# GridForge

Open simulation set-up between FreeCAD and ParaView: mesh, set up, run
and hand over CalculiX analyses, and later OpenFOAM® cases, natively on
Linux. Geometry comes in from FreeCAD as files; results go out to
ParaView as files. Nonlinear solids first: plasticity, with materials
measured by TenSense.

**Status: skeleton (2026-11-14).** Nothing to use yet. What it should be,
and why, is in [docs/survey/](docs/survey/README.md); the plan in
[ROADMAP.md](ROADMAP.md).

## Build

Qt 6, VTK 9, CMake, yaml-cpp and OpenCASCADE's development files (Ubuntu 26.04
packages). Netgen, the meshing engine, is a submodule (our fork,
github.com/katalystnord/netgen) built by the test script at the commit
it pins:

```bash
sudo apt install libocct-foundation-dev libocct-modeling-data-dev \
    libocct-modeling-algorithms-dev libocct-data-exchange-dev libocct-ocaf-dev \
    libyaml-cpp-dev
git submodule update --init
tools/run-tests.sh
```

## Running a study

    gf-study solve setup.json part.step --size H --order 2 -o JOB

meshes the part, writes the job folder, runs the CalculiX toolkit (ccx
on PATH, or where `GF_CCX` names it; `tools/ccx-flatpak` runs FreeCAD's
Flatpak ccx) and leaves the results for ParaView in
`JOB/results/results.vtkhdf`. A worked example:
[examples/dogbone-plastic](examples/dogbone-plastic/README.md).

## Licence

LGPL-2.1-or-later. See [LICENSE](LICENSE) and [NOTICE](NOTICE).

CalculiX, Gmsh and OpenFOAM are separate programs under their own
licences (GPL); GridForge runs them and reads and writes their files,
and does not include their code.

---

This offering is not approved or endorsed by OpenCFD Limited, producer
and distributor of the OpenFOAM software via www.openfoam.com, and owner
of the OPENFOAM® and OpenCFD® trade marks.

OPENFOAM® is a registered trade mark of OpenCFD Limited, producer and
distributor of the OpenFOAM software via www.openfoam.com.
