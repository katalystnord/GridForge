# box-pressure: the box pulled by a pressure

box-setup's tension, with the given displacement replaced by a pressure
of -210 MPa on Pulled (negative: it pulls outward). Written whole by
GridForge (`gf-study deck pressure.json ...box.step --size 5 --order 2`,
the pressure as `*DLOAD`, one line per element face, `P1` to `P4`), solved
by ccx 2.23 on 2026-11-17 with no warning.

Hand answer: box-gfmesh's at every node (210 MPa, ux = 0.001 x, ...); the
reaction on Back (x = 0) is -126000 N in x. That ccx put the load on the
faces it should is shown by the answer itself: a wrong face number would
load the inside of an element.
