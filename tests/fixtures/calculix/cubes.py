#!/usr/bin/env python3
"""Writes one deck per element type: a 10 mm steel cube in tension.

cube-he20/cube.inp (C3D20), cube-te4/cube.inp (C3D4), cube-pe6/cube.inp
(C3D6), cube-pe15/cube.inp (C3D15). Each meshes the cube as 2 x 2 x 2
cells of 5 mm: a hexahedron per cell, six tetrahedra per cell (the Kuhn
split, as cantilever-te10), or two wedges per cell (the cell's square cut
along its diagonal, extruded in z). The quadratic ones have a node in the
middle of every edge.

The load is a uniform strain: z = 0 held in z, x = 0 in x, y = 0 in y,
and z = 10 moved 0.01 mm in z. Every one of these elements reproduces a
linear displacement field exactly (the patch test), so the answer is the
hand answer at every node: ezz = 1e-3, szz = 210 MPa, uz = 1e-3 z,
ux = -3e-4 x, uy = -3e-4 y.

Run by hand when a deck should change: python3 cubes.py, then make.sh.
"""

import os

N, H = 2, 5.0
L = N * H

def write(kind, ccx_type, quadratic, cells_of):
    coords = {}
    corner = {}
    def cid(i, j, k):
        if (i, j, k) not in corner:
            corner[(i, j, k)] = len(coords) + 1
            coords[corner[(i, j, k)]] = (i * H, j * H, k * H)
        return corner[(i, j, k)]
    for k in range(N + 1):
        for j in range(N + 1):
            for i in range(N + 1):
                cid(i, j, k)
    mid = {}
    def m(a, b):
        key = (min(a, b), max(a, b))
        if key not in mid:
            n = len(coords) + 1
            coords[n] = tuple((p + q) / 2 for p, q in zip(coords[a], coords[b]))
            mid[key] = n
        return mid[key]

    elements = []
    for k in range(N):
        for j in range(N):
            for i in range(N):
                for corners in cells_of(i, j, k, cid, coords):
                    elements.append(corners + (quadratic(corners, m) if quadratic else []))

    def on(axis, value):
        return sorted(n for n, x in coords.items() if x[axis] == value)
    def rows(ids, per=8):
        return "\n".join(", ".join(map(str, ids[s:s + per])) for s in range(0, len(ids), per))

    out = [f"** Cube in tension: 10 x 10 x 10 mm, {ccx_type}, written by ../cubes.py.",
           "** Units: mm, N, MPa. Steel, E = 210000 MPa, nu = 0.3.",
           "** z = 0 held in z, x = 0 in x, y = 0 in y; z = 10 moved 0.01 mm in z.",
           "*NODE, NSET=NALL"]
    out += [f"{n}, {x:.1f}, {y:.1f}, {z:.1f}" for n, (x, y, z) in sorted(coords.items())]
    out.append(f"*ELEMENT, TYPE={ccx_type}, ELSET=EALL")
    for i, e in enumerate(elements, 1):
        ids = [str(i)] + [str(n) for n in e]
        # ccx reads at most 16 entries on a line; the rest go on the next.
        out.append(", ".join(ids[:16]) + ("," if len(ids) > 16 else ""))
        if len(ids) > 16:
            out.append(", ".join(ids[16:]))
    out += ["*NSET, NSET=Z0", rows(on(2, 0.0)), "*NSET, NSET=ZL", rows(on(2, L)),
            "*NSET, NSET=X0", rows(on(0, 0.0)), "*NSET, NSET=Y0", rows(on(1, 0.0)),
            "*MATERIAL, NAME=STEEL", "*ELASTIC", "210000.0, 0.3",
            "*SOLID SECTION, ELSET=EALL, MATERIAL=STEEL",
            "*BOUNDARY", "Z0, 3, 3, 0.0", "X0, 1, 1, 0.0", "Y0, 2, 2, 0.0",
            "*STEP", "*STATIC", "*BOUNDARY", "ZL, 3, 3, 0.01",
            "*NODE FILE", "U, RF", "*EL FILE", "S, E", "*END STEP"]
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"cube-{kind}")
    os.makedirs(here, exist_ok=True)
    with open(os.path.join(here, "cube.inp"), "w") as f:
        f.write("\n".join(out) + "\n")

def volume6(coords, a, b, c, d):
    pa, pb, pc, pd = (coords[n] for n in (a, b, c, d))
    u = [pb[t] - pa[t] for t in range(3)]
    v = [pc[t] - pa[t] for t in range(3)]
    w = [pd[t] - pa[t] for t in range(3)]
    return (u[0] * (v[1] * w[2] - v[2] * w[1]) - u[1] * (v[0] * w[2] - v[2] * w[0])
            + u[2] * (v[0] * w[1] - v[1] * w[0]))

# Corners in ccx's order (the Abaqus convention): the bottom face counter-
# clockwise seen from above, then the top face above it.
def hexes(i, j, k, cid, coords):
    return [[cid(i, j, k), cid(i + 1, j, k), cid(i + 1, j + 1, k), cid(i, j + 1, k),
             cid(i, j, k + 1), cid(i + 1, j, k + 1), cid(i + 1, j + 1, k + 1), cid(i, j + 1, k + 1)]]

def wedges(i, j, k, cid, coords):
    a, b, c, d = (i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)
    out = []
    for tri in ((a, b, c), (a, c, d)):
        out.append([cid(*p, k) for p in tri] + [cid(*p, k + 1) for p in tri])
    return out

def tets(i, j, k, cid, coords):
    out = []
    for path in [(0, 1, 2), (0, 2, 1), (1, 0, 2), (1, 2, 0), (2, 0, 1), (2, 1, 0)]:
        p = [i, j, k]
        verts = [cid(*p)]
        for axis in path:
            p[axis] += 1
            verts.append(cid(*p))
        a, b, c, d = verts
        if volume6(coords, a, b, c, d) < 0:
            b, c = c, b
        out.append([a, b, c, d])
    return out

# Midside nodes in ccx's order for each element (ccx manual, C3D20 and C3D15).
def he20_mid(e, m):
    n = e
    return [m(n[0], n[1]), m(n[1], n[2]), m(n[2], n[3]), m(n[3], n[0]),
            m(n[4], n[5]), m(n[5], n[6]), m(n[6], n[7]), m(n[7], n[4]),
            m(n[0], n[4]), m(n[1], n[5]), m(n[2], n[6]), m(n[3], n[7])]

def pe15_mid(e, m):
    n = e
    return [m(n[0], n[1]), m(n[1], n[2]), m(n[2], n[0]),
            m(n[3], n[4]), m(n[4], n[5]), m(n[5], n[3]),
            m(n[0], n[3]), m(n[1], n[4]), m(n[2], n[5])]

write("he20", "C3D20", he20_mid, hexes)
write("te4", "C3D4", None, tets)
write("pe6", "C3D6", None, wedges)
write("pe15", "C3D15", pe15_mid, wedges)
