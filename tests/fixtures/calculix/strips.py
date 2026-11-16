#!/usr/bin/env python3
"""Writes one deck per shell, plane and beam element: a strip in tension.

A 100 x 10 mm strip, 1 mm thick, in the xy plane (a 10 x 1 mm beam for
B31 and B32R): x = 0 held in x, one corner in y, the shells held in z;
x = 100 moved 0.1 mm in x. The hand answer is a uniform strain: exx =
1e-3, sxx = 210 MPa, ux = 1e-3 x, uy = -3e-4 y (plane stress).

What ccx writes into the .frd depends on the element and on OUTPUT=:

    folder        deck element  output     the .frd holds
    shell-s3      S3            OUTPUT=2D  tr3 on the deck's nodes
    shell-s4      S4            OUTPUT=2D  qu4
    shell-s6      S6            OUTPUT=2D  tr6
    shell-s8      S8            OUTPUT=2D  qu8
    shell-s8-3d   S8            (default)  he20 on new nodes: ccx's expansion
    plane-cps4    CPS4          (default)  qu4: plane elements stay 2D
    beam-b31      B31           OUTPUT=2D  be2
    beam-b32r     B32R          OUTPUT=2D  be3

(found with ccx 2.23, 2026-11-16; each folder's README says what its file
holds). Run by hand when a deck should change: python3 strips.py, then
make.sh.
"""

import os

NX, L, W = 4, 100.0, 10.0

FIXTURES = [("shell-s3", "S3", True), ("shell-s4", "S4", True), ("shell-s6", "S6", True),
            ("shell-s8", "S8", True), ("shell-s8-3d", "S8", False), ("plane-cps4", "CPS4", False),
            ("beam-b31", "B31", True), ("beam-b32r", "B32R", True)]


def strip(kind):
    """Nodes {id: (x, y, z)} and elements (lists of node ids, ccx's order)."""
    if kind.startswith("B"):
        quadratic = kind != "B31"
        n = NX * (2 if quadratic else 1)
        nodes = {i + 1: (L * i / n, 0.0, 0.0) for i in range(n + 1)}
        if quadratic:  # end, middle, end
            elements = [[2 * i + 1, 2 * i + 2, 2 * i + 3] for i in range(NX)]
        else:
            elements = [[i + 1, i + 2] for i in range(NX)]
        return nodes, elements
    q = 2 if kind in ("S6", "S8") else 1
    grid = {}
    for j in range(q + 1):
        for i in range(NX * q + 1):
            grid[(i, j)] = (L * i / (NX * q), W * j / q, 0.0)
    ids = {key: n + 1 for n, key in enumerate(sorted(grid, key=lambda k: (k[1], k[0])))}
    elements = []
    for i in range(NX):
        a = q * i
        c = lambda di, dj: ids[(a + di, dj)]
        if kind in ("S4", "CPS4"):
            elements.append([c(0, 0), c(1, 0), c(1, 1), c(0, 1)])
        elif kind == "S3":
            elements += [[c(0, 0), c(1, 0), c(1, 1)], [c(0, 0), c(1, 1), c(0, 1)]]
        elif kind == "S8":  # corners, then the middles of edges 1-2, 2-3, 3-4, 4-1
            elements.append([c(0, 0), c(2, 0), c(2, 2), c(0, 2),
                             c(1, 0), c(2, 1), c(1, 2), c(0, 1)])
        elif kind == "S6":  # corners, then the middles of edges 1-2, 2-3, 3-1
            elements += [[c(0, 0), c(2, 0), c(2, 2), c(1, 0), c(2, 1), c(1, 1)],
                         [c(0, 0), c(2, 2), c(0, 2), c(1, 1), c(1, 2), c(0, 1)]]
    used = sorted({n for e in elements for n in e})
    nodes = {ids[k]: x for k, x in grid.items() if ids[k] in used}
    return nodes, elements


def deck(kind, out2d):
    nodes, elements = strip(kind)
    at = lambda x: sorted(n for n, p in nodes.items() if p[0] == x)
    what = {"S": "shell", "C": "plane stress", "B": "beam"}[kind[0]]
    out = [f"** Strip in tension, {kind} ({what}), written by ../strips.py.",
           "** Units: mm, N, MPa. Steel, E = 210000 MPa, nu = 0.3.",
           "** x = 0 held in x, x = 100 moved 0.1 mm in x.",
           "*NODE, NSET=NALL"]
    out += [f"{n}, {x:.2f}, {y:.2f}, {z:.2f}" for n, (x, y, z) in sorted(nodes.items())]
    out.append(f"*ELEMENT, TYPE={kind}, ELSET=EALL")
    out += [f"{i}, " + ", ".join(map(str, e)) for i, e in enumerate(elements, 1)]
    out += ["*NSET, NSET=X0", ", ".join(map(str, at(0.0))),
            "*NSET, NSET=XL", ", ".join(map(str, at(L))),
            "*MATERIAL, NAME=STEEL", "*ELASTIC", "210000.0, 0.3"]
    if kind.startswith("B"):
        out += ["*BEAM SECTION, ELSET=EALL, MATERIAL=STEEL, SECTION=RECT", "10.0, 1.0",
                "0.0, 0.0, 1.0", "*BOUNDARY", "1, 1, 6, 0.0"]
    elif kind.startswith("S"):
        out += ["*SHELL SECTION, ELSET=EALL, MATERIAL=STEEL", "1.0",
                "*BOUNDARY", "X0, 1, 1, 0.0", "1, 2, 2, 0.0", "NALL, 3, 3, 0.0"]
    else:
        out += ["*SOLID SECTION, ELSET=EALL, MATERIAL=STEEL", "1.0",
                "*BOUNDARY", "X0, 1, 1, 0.0", "1, 2, 2, 0.0"]
    option = ", OUTPUT=2D" if out2d else ""
    out += ["*STEP", "*STATIC", "*BOUNDARY", "XL, 1, 1, 0.1",
            f"*NODE FILE{option}", "U, RF", f"*EL FILE{option}", "S, E", "*END STEP"]
    return "\n".join(out) + "\n"


here = os.path.dirname(os.path.abspath(__file__))
for folder, kind, out2d in FIXTURES:
    os.makedirs(os.path.join(here, folder), exist_ok=True)
    with open(os.path.join(here, folder, "strip.inp"), "w") as f:
        f.write(deck(kind, out2d))
