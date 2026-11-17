# Gmsh meshes

Written by Gmsh 4.14.0 (Ubuntu's `gmsh` package, 4.14.0+ds1: the release's
source, built without `GMSH_RELEASE`, so it calls itself "4.14.0-git") from
the committed `.geo` files, by `make.sh`. Each folder's `gmsh.log` says the
command and what Gmsh counted. The tests read only the committed `.msh`.

Every fixture is the same box, 10 x 20 x 30 mm, with the same physical
groups:

| group | dimension | tag | what |
|---|---|---|---|
| Solid | 3 | 1 | the box |
| Fixed | 2 | named | the face x = 0 |
| Pulled | 2 | named | the face x = 10 |
| (none) | 2 | 77 | the top, z = 30: Gmsh names it PhysicalSurface77 in its own .inp export, and so do we |
| Edge | 1 | named | the edge along x at y = z = 0 |
| Origin | 0 | named | the corner at the origin |

Gmsh saves only the elements of physical groups (its default), so each
file holds the volume elements and the faces, edge and point of the
groups.

## The answers, by hand

Whatever the elements, the box encloses 6000 mm3, every node of Fixed has
x = 0, of Pulled x = 10, of the top z = 30, of Edge y = z = 0, and Origin
is one node at (0, 0, 0).

| folder | from | elements | nodes, by hand |
|---|---|---|---|
| box-hex8 | hex.geo | 24 hex8 (2 x 3 x 4), 30 quad4, 2 line2, 1 point | 3 x 4 x 5 = 60 |
| box-hex8-binary | hex.geo, `-bin` | the same, in binary | 60 |
| box-hex20 | hex.geo, order 2, incomplete | 24 hex20, 30 quad8, 2 line3, 1 point | 60 corners, 40 + 45 + 48 edge middles: 193 |
| box-prism6 | prism.geo | 48 prism6 (12 triangles x 4 layers), 24 quad4, 12 tri3, 2 line2, 1 point | 60 |
| box-prism15 | prism.geo, order 2, incomplete | 48 prism15, 24 quad8, 12 tri6, 2 line3, 1 point | 60 corners, 23 edges in each of 5 levels, 12 upright edges in each of 4 layers: 223 |
| box-tet4 | tet.geo | 897 tet4, 306 tri3, 3 line2, 1 point | not by hand (294); the volume is the check |
| box-tet10 | tet.geo, order 2 | 897 tet10, 306 tri6, 3 line3, 1 point | not by hand (1736) |

Node sets by hand, box-hex8: Fixed and Pulled 4 x 5 = 20, the top 3 x 4 =
12, Edge 3, Origin 1. box-hex20: Fixed holds 20 corners and 15 + 16 edge
middles, 51.

## Refused

- `refused/hex27`: Gmsh's complete second order (`-order 2` alone): 27-node
  hexahedra with 9-node quadrangles on the faces. ccx has neither. The
  reader names the first type it meets, the quadrangle (type 10).
- `refused/msh22`: the older MSH 2.2 format.
