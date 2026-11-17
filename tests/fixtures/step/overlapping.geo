// Two 10 x 10 x 10 mm boxes that overlap by half (x 0 to 10 and 5 to 15),
// as two solids: a CAD fault, since a point would belong to both.
SetFactory("OpenCASCADE");
Box(1) = {0, 0, 0, 10, 10, 10};
Box(2) = {5, 0, 0, 10, 10, 10};
