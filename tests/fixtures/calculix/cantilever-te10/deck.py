#!/usr/bin/env python3
"""Writes cantilever.inp: a 10 x 10 x 100 mm steel cantilever in C3D10.

A grid of 2 x 2 x 20 cubes (5 mm), each cut into six tetrahedra around its
main diagonal (the Kuhn split, conforming between neighbours), with a node
at the middle of every edge. Root z = 0 held; tip z = 100 loaded with 100 N
in y, spread as a uniform shear traction: on a 6-node triangle that puts
a third of the triangle's share on each midside node and none on corners.
Run by hand when the deck should change: python3 deck.py > cantilever.inp
"""

NX, NY, NZ = 2, 2, 20
H = 5.0
P = 100.0

corner = {}
def cid(i, j, k):
    return corner.setdefault((i, j, k), len(corner) + 1)
for k in range(NZ + 1):
    for j in range(NY + 1):
        for i in range(NX + 1):
            cid(i, j, k)
coords = {n: (i * H, j * H, k * H) for (i, j, k), n in corner.items()}

mid = {}
def mid_node(a, b):
    key = (min(a, b), max(a, b))
    if key not in mid:
        n = len(coords) + 1
        coords[n] = tuple((p + q) / 2 for p, q in zip(coords[a], coords[b]))
        mid[key] = n
    return mid[key]

def volume6(a, b, c, d):
    pa, pb, pc, pd = (coords[n] for n in (a, b, c, d))
    u = [pb[t] - pa[t] for t in range(3)]
    v = [pc[t] - pa[t] for t in range(3)]
    w = [pd[t] - pa[t] for t in range(3)]
    return (u[0] * (v[1] * w[2] - v[2] * w[1]) - u[1] * (v[0] * w[2] - v[2] * w[0])
            + u[2] * (v[0] * w[1] - v[1] * w[0]))

# Kuhn: the six monotone paths from (0,0,0) to (1,1,1) through the cube.
paths = [(0, 1, 2), (0, 2, 1), (1, 0, 2), (1, 2, 0), (2, 0, 1), (2, 1, 0)]
elements = []
for k in range(NZ):
    for j in range(NY):
        for i in range(NX):
            for path in paths:
                p = [i, j, k]
                verts = [cid(*p)]
                for axis in path:
                    p[axis] += 1
                    verts.append(cid(*p))
                a, b, c, d = verts
                if volume6(a, b, c, d) < 0:
                    b, c = c, b
                elements.append([a, b, c, d,
                                 mid_node(a, b), mid_node(b, c), mid_node(c, a),
                                 mid_node(a, d), mid_node(b, d), mid_node(c, d)])

root = sorted(n for n, x in coords.items() if x[2] == 0.0)
tip_load = {}
area = (NX * H) * (NY * H)
for e in elements:
    face = [n for n in e[:4] if coords[n][2] == NZ * H]
    if len(face) == 3:
        a, b, c = face
        (xa, ya, _), (xb, yb, _), (xc, yc, _) = coords[a], coords[b], coords[c]
        share = P * abs((xb - xa) * (yc - ya) - (xc - xa) * (yb - ya)) / 2 / area
        for m in (mid_node(a, b), mid_node(b, c), mid_node(c, a)):
            tip_load[m] = tip_load.get(m, 0.0) + share / 3
tip = sorted(n for n, x in coords.items() if x[2] == NZ * H)

def rows(ids, per=8):
    return "\n".join(", ".join(str(n) for n in ids[s:s + per]) for s in range(0, len(ids), per))

out = ["** Cantilever: 10 x 10 x 100 mm, C3D10, written by deck.py.",
       "** Units: mm, N, MPa. Steel, E = 210000 MPa, nu = 0.3.",
       "** Root z = 0 held in x, y and z; 100 N in y on the tip z = 100.",
       "*NODE, NSET=NALL"]
out += [f"{n}, {x:.1f}, {y:.1f}, {z:.1f}" for n, (x, y, z) in sorted(coords.items())]
out.append("*ELEMENT, TYPE=C3D10, ELSET=EALL")
out += [f"{i}, " + ", ".join(map(str, e)) for i, e in enumerate(elements, 1)]
out += ["*NSET, NSET=ROOT", rows(root), "*NSET, NSET=TIP", rows(tip),
        "*MATERIAL, NAME=STEEL", "*ELASTIC", "210000.0, 0.3",
        "*SOLID SECTION, ELSET=EALL, MATERIAL=STEEL",
        "*BOUNDARY", "ROOT, 1, 3, 0.0",
        "*STEP", "*STATIC", "*CLOAD"]
out += [f"{n}, 2, {f:.10g}" for n, f in sorted(tip_load.items())]
out += ["*NODE FILE", "U, RF", "*EL FILE", "S, E", "*END STEP"]
print("\n".join(out))
