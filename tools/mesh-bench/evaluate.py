"""The benchmark's referee: the same measures for every mesher's output.

    evaluate.py mesh-file reference-volume  ->  JSON on stdout

Volume elements only (surfaces, lines and points are left out). Measures:
  cells by type; nodes used;
  volume and its error against the CAD volume (corners only: the
    straight-sided volume, so curved faces show as a small error);
  for tetrahedra (corner nodes): the mean ratio, 12 (3V)^(2/3) / sum of
    squared edge lengths, 1 for the regular tetrahedron and 0 for a flat
    one; its minimum, mean, and the share below 0.1 and 0.3; the smallest
    dihedral angle;
  for 10-node tetrahedra: how many have a Jacobian determinant <= 0 at
    any corner, mid-edge node or the centroid (inverted or folded);
  inverted linear cells (negative corner volume);
  a hash of the node coordinates and the connectivity, for repeatability.
"""
import hashlib
import json
import sys

import meshio
import numpy as np

VOLUME_TYPES = {"tetra", "tetra10", "hexahedron", "hexahedron20", "hexahedron27", "wedge",
                "wedge15", "pyramid", "pyramid13", "polyhedron"}


def tet_corner_volumes(p, t):
    a, b, c, d = (p[t[:, k]] for k in range(4))
    return np.einsum("ij,ij->i", np.cross(b - a, c - a), d - a) / 6.0


def mean_ratio(p, t):
    v = np.abs(tet_corner_volumes(p, t))
    s = np.zeros(len(t))
    for i, j in ((0, 1), (0, 2), (0, 3), (1, 2), (1, 3), (2, 3)):
        s += np.sum((p[t[:, i]] - p[t[:, j]]) ** 2, axis=1)
    return 12.0 * (3.0 * v) ** (2.0 / 3.0) / s


def min_dihedral(p, t):
    """The smallest dihedral angle of each tetrahedron, in degrees: at each
    edge, 180 minus the angle between the two faces' outward normals."""
    n = []
    for k in range(4):
        f = [i for i in range(4) if i != k]
        a, b, c = (p[t[:, i]] for i in f)
        x = np.cross(b - a, c - a)
        # Outward: away from the vertex opposite the face.
        flip = np.einsum("ij,ij->i", x, p[t[:, k]] - a) > 0
        x[flip] *= -1
        n.append(x / np.maximum(np.linalg.norm(x, axis=1, keepdims=True), 1e-300))
    worst = np.full(len(t), 180.0)
    for i in range(4):
        for j in range(i + 1, 4):
            cos = np.einsum("ij,ij->i", n[i], n[j])
            worst = np.minimum(worst, 180.0 - np.degrees(np.arccos(np.clip(cos, -1, 1))))
    return worst


def tet10_bad(p, t):
    """10-node tetrahedra (meshio/VTK order: corners, then 01 12 20 03 13 23)
    with a Jacobian determinant <= 0 at a node or the centroid."""
    pts = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (.5, 0, 0), (.5, .5, 0), (0, .5, 0),
           (0, 0, .5), (.5, 0, .5), (0, .5, .5), (.25, .25, .25)]
    X = p[t]  # (n, 10, 3)
    orient = np.sign(tet_corner_volumes(p, t[:, :4]))
    bad = np.zeros(len(t), bool)
    for r, s, u in pts:
        L = np.array([1 - r - s - u, r, s, u])
        dL = np.array([[-1, -1, -1], [1, 0, 0], [0, 1, 0], [0, 0, 1]], float)
        # Quadratic shape-function gradients for the VTK order.
        dN = np.zeros((10, 3))
        for k in range(4):
            dN[k] = (4 * L[k] - 1) * dL[k]
        for k, (i, j) in enumerate(((0, 1), (1, 2), (2, 0), (0, 3), (1, 3), (2, 3))):
            dN[4 + k] = 4 * (L[i] * dL[j] + L[j] * dL[i])
        J = np.einsum("nki,kj->nij", X, dN)
        bad |= np.linalg.det(J) * orient <= 0
    return int(bad.sum())


def tet10_volume(p, t):
    """The volume of 10-node tetrahedra with their middle nodes where they
    are (curved), by a 4-point rule (exact for straight cells, for
    curved cells only approximately; the straight boxes give 6000 to
    1e-15)."""
    a, b = 0.5854101966249685, 0.1381966011250105
    rule = [(a, b, b), (b, a, b), (b, b, a), (b, b, b)]
    X = p[t]
    total = np.zeros(len(t))
    dL = np.array([[-1, -1, -1], [1, 0, 0], [0, 1, 0], [0, 0, 1]], float)
    for r, s_, u in rule:
        L = np.array([1 - r - s_ - u, r, s_, u])
        dN = np.zeros((10, 3))
        for k in range(4):
            dN[k] = (4 * L[k] - 1) * dL[k]
        for k, (i, j) in enumerate(((0, 1), (1, 2), (2, 0), (0, 3), (1, 3), (2, 3))):
            dN[4 + k] = 4 * (L[i] * dL[j] + L[j] * dL[i])
        total += np.linalg.det(np.einsum("nki,kj->nij", X, dN)) / 24.0
    return total


def boundary_area(p, tets):
    """The area of the faces used by one tetrahedron only: the outside,
    plus any interface whose two sides do not share nodes."""
    f = np.vstack([tets[:, [1, 2, 3]], tets[:, [0, 2, 3]], tets[:, [0, 1, 3]], tets[:, [0, 1, 2]]])
    key = np.sort(f, axis=1)
    _, idx, count = np.unique(key, axis=0, return_index=True, return_counts=True)
    once = f[idx[count == 1]]
    a, b, c = p[once[:, 0]], p[once[:, 1]], p[once[:, 2]]
    return float(0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1).sum())


def hex_scaled_jacobian(p, c):
    """The smallest scaled Jacobian over each hexahedron's 8 corners (VTK
    order): the triple product of the three unit edges at the corner; 1
    for a cuboid, 0 or less for a collapsed or inverted corner."""
    nb = [(1, 3, 4), (2, 0, 5), (3, 1, 6), (0, 2, 7), (7, 5, 0), (4, 6, 1), (5, 7, 2), (6, 4, 3)]
    worst = np.full(len(c), np.inf)
    for k, (a, b, d) in enumerate(nb):
        e = [p[c[:, j]] - p[c[:, k]] for j in (a, b, d)]
        e = [x / np.maximum(np.linalg.norm(x, axis=1, keepdims=True), 1e-300) for x in e]
        worst = np.minimum(worst, np.einsum("ij,ij->i", np.cross(e[0], e[1]), e[2]))
    return worst


def evaluate(path, reference):
    m = meshio.read(path)
    p = np.asarray(m.points, float)
    out = {"cells": {}, "nodes_used": 0}
    used = set()
    signed = []
    tets = []
    hexes = []
    h = hashlib.sha256()
    h.update(np.round(p, 9).tobytes())
    for block in m.cells:
        if block.type not in VOLUME_TYPES:
            continue
        c = np.asarray(block.data)
        out["cells"][block.type] = out["cells"].get(block.type, 0) + len(c)
        h.update(block.type.encode() + c.tobytes())
        used.update(np.unique(c).tolist())
        if block.type in ("tetra", "tetra10"):
            v = tet_corner_volumes(p, c[:, :4])
            signed.append(v)
            tets.append(c[:, :4])
            if block.type == "tetra10":
                out["volume_curved"] = out.get("volume_curved", 0.0) + abs(float(tet10_volume(p, c).sum()))
                out["tet10_bad_jacobian"] = out.get("tet10_bad_jacobian", 0) + tet10_bad(p, c)
        if block.type in ("hexahedron", "hexahedron20"):
            sj = hex_scaled_jacobian(p, c[:, :8])
            hexes.append(sj)
        if block.type in ("hexahedron", "hexahedron20", "wedge", "wedge15", "pyramid", "pyramid13"):
            # Split into tetrahedra about the cell centre (corners only).
            corners = {"hexahedron": 8, "hexahedron20": 8, "wedge": 6, "wedge15": 6,
                       "pyramid": 5, "pyramid13": 5}[block.type]
            faces = {8: [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)],
                     6: [(0, 2, 1), (3, 4, 5), (0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5)],
                     5: [(0, 3, 2, 1), (0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]}[corners]
            cc = p[c[:, :corners]].mean(axis=1)
            cv = np.zeros(len(c))
            for f in faces:
                a = p[c[:, f[0]]]
                for k in range(1, len(f) - 1):
                    b, e = p[c[:, f[k]]], p[c[:, f[k + 1]]]
                    cv += np.einsum("ij,ij->i", np.cross(b - a, e - a), cc - a) / 6.0
            signed.append(-cv)
    out["nodes_used"] = len(used)
    v = np.concatenate(signed) if signed else np.zeros(0)
    sign = 1.0 if v.sum() >= 0 else -1.0
    volume = abs(v.sum())
    inverted = int((v * sign <= 0).sum())
    out["volume"] = volume
    out["volume_error"] = (volume - reference) / reference
    out["inverted"] = inverted
    if tets:
        t = np.vstack(tets)
        q = mean_ratio(p, t)
        out["tet_quality"] = {"min": float(q.min()), "mean": float(q.mean()),
                              "share_below_0.1": float((q < 0.1).mean()),
                              "share_below_0.3": float((q < 0.3).mean())}
        out["tet_min_dihedral_deg"] = float(min_dihedral(p, t).min())
        out["boundary_area"] = boundary_area(p, t)
    if hexes:
        sj = np.concatenate(hexes)
        # The mesh's orientation decides the sign, as for the volume.
        sj = sj * (1.0 if np.median(sj) >= 0 else -1.0)
        out["hex_scaled_jacobian"] = {"min": float(sj.min()), "mean": float(sj.mean()),
                                      "share_below_0.2": float((sj < 0.2).mean())}
    if "volume_curved" in out:
        out["volume_curved_error"] = (out["volume_curved"] - reference) / reference
    out["hash"] = h.hexdigest()[:16]
    return out


if __name__ == "__main__":
    print(json.dumps(evaluate(sys.argv[1], float(sys.argv[2]))))
