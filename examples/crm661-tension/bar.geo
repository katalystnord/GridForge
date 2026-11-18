// A quarter of TENSTAND's round test piece's parallel length (NPL, file
// 06: CRM 661, So 78.54 mm2, so 10 mm across; Lc 60 mm), along x from 0
// to 60, in y <= 0 and z >= 0 (as Gmsh 4.14 turns it). The cut planes are
// symmetry planes.
SetFactory("OpenCASCADE");
Cylinder(1) = {0, 0, 0, 60, 0, 0, 5, Pi/2};
