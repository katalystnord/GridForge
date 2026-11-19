# CRM 661 pulled: measured against predicted

A tensile test of a certified reference material, measured, beside the
same test predicted by GridForge and ccx from TenSense's analysis of it.

- **Measured:** NPL's TENSTAND file 06 (National Physical Laboratory,
  the TENSTAND project's Premium Quality ASCII datafiles; NPL Report DEPC
  MPE 015, 2005): CRM 661, Nimonic 75, a round test piece of So 78.54 mm2
  (10 mm across), parallel length Lc 60 mm, an extensometer over Le
  50 mm to fracture. NPL states no licence for the files, so `run.sh`
  fetches them (and checks their SHA-256), and nothing of them, nor
  anything derived from them, is committed here. TenSense's own
  validation does the same.
- **Analysed** by TenSense's `ts-analyse` (iso-6892-1-2019): the curve
  (`inputs/06.vtp`), the material card (E) and the hardening curve, true
  stress against true plastic strain from Rp0.2 to the maximum force
  (`inputs/CRM661-plastic.inp`, TenSense's exports spec, section 1).
- **Predicted:** a quarter of the parallel length (`bar.geo`, `bar.step`):
  the x = 0 end and the two cut planes held by symmetry, so the bar
  thins freely; the x = 60 end moved 18.9 mm (31.5 % of 60, the uniform
  elongation TenSense found); Poisson's ratio 0.3, given (a tensile test
  does not measure it); large displacements, 52 increments; meshed at
  2 mm, quadratic. Then `gf-study compare`: the force on the moved end
  (times 4, the quarter) against a virtual extensometer from x = 5 to
  x = 55, beside the measured curve.

    ./run.sh

(TenSense's `ts-analyse` is found on PATH or in its build beside
GridForge, `../TenSense/build-ninja/src/ts-analyse`, else set
`TS_ANALYSE`; ccx on PATH or GridForge's own build, else set `GF_CCX`.)

## What it gave (2026-11-18, ccx 2.23, TenSense 0.3.0, 51 s)

    predicted against measured, at 52 strains from 0.315 % to 31.5 %:
    at most 1.17 % apart (under, at 1.1 %), 0.373 % on average

| strain | predicted | measured | difference |
|---|---|---|---|
| 0.32 % | 24249 N | 24009 N | +1.0 % |
| 3.6 % | 35660 N | 35594 N | +0.19 % |
| 11.2 % | 48650 N | 48490 N | +0.33 % |
| 22.5 % | 58111 N | 57888 N | +0.39 % |
| 31.5 % | 59997 N | 59762 N | +0.39 % |

(`compare.csv` has every point.) The largest differences sit where the
curve turns from elastic to plastic (0.3 to 1.1 %): the model yields
sharply at Rp0.2, the metal gradually. From 3 % on, the prediction runs
0.2 to 0.4 % above the measurement. Why is not found yet; candidates not
checked are Poisson's ratio (given, not measured) and the hardening
curve's 20 points, linear between.

## A second test, predicted (2026-11-18)

TENSTAND's file 01 is another round test piece of "Nimonic" (its file
says no more; S0 78.461 mm2, within 0.10 % of file 06's, the same Lc
and Le; TenSense finds Rp0.2 310 MPa and Rm 764 MPa, against 308 and
761 for file 06). The same solve, from file 06's analysis, set beside
file 01's measurement (`compare-01.csv`):

    predicted against measured, at 52 strains from 0.315 % to 31.5 %:
    at most 0.687 % apart (under, at 1.73 %), 0.214 % on average

This one is a prediction: nothing of file 01 went into the model. Not
checked: that file 01 is CRM 661 itself, and the same batch. The model's
section is file 06's, 0.10 % larger than file 01's, and not corrected
for.

## What this shows, and what it does not

It shows the chain holds: TenSense's analysis, its exports, GridForge's
mesh, set-up, deck and results, and ccx's large-strain plasticity give
back the force the test piece carried, from yield to the maximum force,
to within half a percent. Against file 06 it is a **calibration check**,
not a validation: the material model was fitted to that same test, so
agreement is what a correct chain must give. Against file 01 it is a
prediction of a second test of the same kind; one of another shape (a
notched piece, or bending) from this material is the step after.
Necking (after the maximum force) is not modelled.
