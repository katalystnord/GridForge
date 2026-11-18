# A dogbone bent, checked against beam theory

The dogbone from `../dogbone-plastic` (150 x 20 x 4 mm, narrow part
60 x 10 mm, R30 transitions), elastic steel (E = 199.9 GPa, Poisson's
ratio 0.3), clamped at one grip (face 12, x = 0) and bent through its
4 mm thickness at the other (face 6, x = 150):

- `push.json`: a force of 10 N in -z, `"direction": "-z"`;
- `move.json`: a movement of 2 mm in -z, x and y left free.

Both are what the window's load form writes for those choices (pick
face 6; Force or Move; Z, clicked twice for -z; the size).

`study.json` is the push as a study for the window: open the folder in
GridForge, mesh, Run, and the results show the bend (Exaggerate, beside
the Deform box, turns the view across it).

    GF_CCX=../../tools/ccx-flatpak \
        ../../build-ninja/src/gf-study solve push.json dogbone.step --size 3 --order 2 -o job-push
    GF_CCX=../../tools/ccx-flatpak \
        ../../build-ninja/src/gf-study solve move.json dogbone.step --size 3 --order 2 -o job-move
    python3 check.py job-push job-move

`check.py` reads ccx's own `.frd`, not GridForge's converted results, and
works out the hand answer itself: Euler-Bernoulli for a cantilever with
the dogbone's real width along it (the arcs' centres and radius are
the STEP's), integrated numerically.

## What it gave (2026-11-18, ccx 2.23, 1624 C3D10, 3575 nodes)

| | ccx | hand answer |
|---|---|---|
| push, tip deflection | -0.75584 mm (45 tip nodes, within 0.00004 of each other) | -0.75708 mm (-0.16 %) |
| push, reaction at the clamp | 9.9999 N | 10 N |
| move, tip | -2.00000 mm; x and y about 0 | |
| move, reaction at the clamp | 26.4610 N | 10 N x 2 / 0.75584 mm = 26.4608 N |

The deck holds `*CLOAD` on face 6's nodes in z for the push (summing to
-10 N) and `Pushed_grip, 3, 3, -2` for the move, with no line for x or y.
The `"direction"` string is a record of how the load was given; the deck
writer does not read it.

`check.py` was seen to fail with the two jobs swapped (+164 %).

## Again, with two elements through the thickness (2026-11-18)

With the mesh recipe's `throughWall` (study spec 2.1, default 2), the
4 mm thickness is meshed at 1.33 mm: 24108 C3D10, 39186 nodes.

| | 1624 elements | 24108 elements | hand answer |
|---|---|---|---|
| push, tip deflection | -0.75584 mm (-0.16 %) | -0.75785 mm (+0.10 %) | -0.75708 mm |
| move, reaction at the clamp | 26.4610 N | 26.3904 N | the push's stiffness x 2 mm: 26.3903 N |

One element through the thickness was too stiff, as such a mesh is in
bending. Two land on the side beam theory should be on: it leaves out
the shear, which bends a real cantilever a little more.

## How far the mesh can be trusted (2026-11-18)

`gf-study converge` solves the push on three meshes, each finer than the
last in every size the recipe makes (`finer`, study spec 2.1), and says
how far the largest displacement and the largest von Mises stress can be
trusted against the mesh (core/Convergence: Richardson extrapolation and
the grid convergence index of Celik et al. 2008):

    GF_CCX=../../build-ccx/install/bin/ccx \
        ../../build-ninja/src/gf-study converge push.json dogbone.step \
        --size 5 --order 2 --through-wall 1 -o job-converge

With ccx 2.23, in 40 s on this machine:

| every size over | elements | tip deflection | largest von Mises |
|---|---|---|---|
| 1 | 6210 | 0.757639 mm | 41.7196 MPa |
| 1.5 | 24108 | 0.758042 mm | 41.9616 MPa |
| 2.25 | 49198 | 0.758185 mm | 41.879 MPa |

- The tip deflection converges (order 1.1, from the element sizes'
  ratios the counts give, 1.572 then 1.268: Netgen does not shrink every
  element by the ratio asked), to 0.75866 mm at zero element size; the
  finest mesh is within 0.078 % of that. Beam theory's 0.75708 mm is
  0.21 % below it, on the side it should be: it leaves out the shear.
- The largest von Mises stress oscillates (up 0.6 %, then down 0.2 %):
  no order to extrapolate with. It sits where the grip is clamped, a
  sharp change from held to free, where the peak depends on the mesh.
