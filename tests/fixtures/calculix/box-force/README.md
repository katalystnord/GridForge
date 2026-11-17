# box-force: the box pulled by a force

box-setup's tension, with the given displacement replaced by a total
force of 126000 N in x on Pulled. Written whole by GridForge: the force
spread over the face's nodes by the element faces' areas (on these
quadratic faces, a third of each face's area to each middle node, none
to the corners: the consistent load of a uniform traction), as `*CLOAD`.
Solved by ccx 2.23 on 2026-11-17 with no warning.

Hand answer: box-gfmesh's at every node; the reaction on Back (x = 0) is
-126000 N in x.
