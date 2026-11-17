// A box 10 x 20 x 30 mm, unstructured tetrahedra up to 5 mm. Physical
// groups: the solid, the faces x = 0 and x = 10, the top z = 30 (tag 77,
// no name), the edge along x at y = z = 0, the corner at the origin.
// Meshed by make.sh.
SetFactory("OpenCASCADE");
Box(1) = {0, 0, 0, 10, 20, 30};
Mesh.MeshSizeMax = 5;
Physical Volume("Solid") = {1};
e = 1e-6;
Physical Surface("Fixed") = Surface In BoundingBox{-e, -e, -e, e, 20 + e, 30 + e};
Physical Surface("Pulled") = Surface In BoundingBox{10 - e, -e, -e, 10 + e, 20 + e, 30 + e};
Physical Surface(77) = Surface In BoundingBox{-e, -e, 30 - e, 10 + e, 20 + e, 30 + e};  // unnamed: the top
Physical Curve("Edge") = Curve In BoundingBox{-e, -e, -e, 10 + e, e, e};
Physical Point("Origin") = Point In BoundingBox{-e, -e, -e, e, e, e};
