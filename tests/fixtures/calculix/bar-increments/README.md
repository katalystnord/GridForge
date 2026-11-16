# Bar in tension, four increments

`bar.inp`: `../bar-tension/bar.inp` with `*STEP, NLGEOM` and `*STATIC,
DIRECT` at 0.25, so ccx takes four fixed increments and writes the
results of each. (Without NLGEOM a linear step is solved in one
increment and the DIRECT increments are ignored: checked.)

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16.

## The known answer (by hand)

The end is moved, so at increment n (n = 1 to 4, time 0.25 n) the tip
uz is exactly 0.025 n mm. The reaction is 5250 n N, as for the linear bar,
to within the geometric nonlinearity (strains of 1e-3: well under 0.1 %).

## What the file holds

- 20 datasets: DISP, STRESS, TOSTRAIN, FORC, ERROR at each increment.
- The 100C record's step field (NUMSTP) counts increments: 1, 2, 3, 4,
  with the time (0.25, 0.5, 0.75, 1.0) as its value. Each dataset's
  `1PSTEP` record holds three numbers: the dataset's running count (1 to
  20), the increment and the step (1).
- Tip uz at node 20: 0.025, 0.05, 0.075, 0.1, exactly. The reaction on
  nodes 17 to 20: 5251.96, 10507.9, 15767.7, 21031.5 N (0.04 % to 0.15 %
  above linear, from NLGEOM).
