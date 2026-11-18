# STEP fixtures for gf_mesh

Geometry for the Netgen mesher's tests. Each has an answer known without
any mesher.

| file | what | known answer |
|---|---|---|
| `box.step` | a 10 x 20 x 30 mm box at the origin, one solid | 6000 mm3; 2200 mm2; six faces |
| `twoboxes.step` | two 10 mm cubes side by side, x 0 to 10 and 10 to 20, two solids touching at x = 10 | 1000 mm3 each |
| `overlapping.step` | two 10 mm cubes, x 0 to 10 and 5 to 15, two solids that overlap by half | a fault: 500 mm3 would belong to both |
| `tworoots.step` | two 10 mm cubes apart, x 0 to 10 and 20 to 30, each its own root in the file | 1000 mm3 each, two solids |
| `dogbone.step` | the benchmark's dogbone: 150 x 20 x 4 mm, narrow part 60 x 10, R30 transitions | 8699.688877463912 mm3, OpenCASCADE's volume of the solid (`tools/mesh-bench/models/models.json`) |

`box.step`, `twoboxes.step` and `overlapping.step` come from the `.geo` files through Gmsh
4.14's OpenCASCADE 7.9 (`make.sh`); `dogbone.step` from
`tools/mesh-bench/models.py`; `tworoots.step` from `tworoots.cpp`, OpenCASCADE 7.9's
STEP writer given one body at a time (`make.sh`).
