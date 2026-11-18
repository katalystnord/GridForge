// A plate 20 x 10 x 1.5 mm at the origin: its top and bottom faces 1.5 mm
// apart, its sides 10 and 20 mm from the side opposite. 300 mm3.
SetFactory("OpenCASCADE");
Box(1) = {0, 0, 0, 20, 10, 1.5};
