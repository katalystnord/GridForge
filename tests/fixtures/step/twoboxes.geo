// Two 10 x 10 x 10 mm boxes side by side (x 0 to 10 and 10 to 20), as two
// solids that touch at x = 10: 1000 mm3 each.
SetFactory("OpenCASCADE");
Box(1) = {0, 0, 0, 10, 10, 10};
Box(2) = {10, 0, 0, 10, 10, 10};
