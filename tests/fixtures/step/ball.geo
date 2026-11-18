// A ball of radius 10 mm at the origin: 4188.79 mm3 (4/3 pi r^3). Its
// surface closes, though OpenCASCADE gives it a seam (an edge bordering
// the one face on both sides) and two poles (degenerate edges).
SetFactory("OpenCASCADE");
Sphere(1) = {0, 0, 0, 10};
