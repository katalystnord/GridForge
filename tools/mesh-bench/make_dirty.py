"""models/bracket-dirty.stl: the bracket's surface damaged as exported CAD
often is: corners that no longer meet (each triangle's corners nudged up
to 0.002 mm), 12 triangles missing, and 40 triangles doubled with a 0.01
mm offset. Seeded, so the same file every time."""
import numpy as np, meshio
rng = np.random.default_rng(20261117)
m = meshio.read("models/bracket.stl")
p, t = m.points, m.cells_dict["triangle"]
tri = p[t] + rng.uniform(-0.002, 0.002, (len(t), 3, 3))
keep = np.ones(len(tri), bool); keep[rng.choice(len(tri), 12, replace=False)] = False
tri = tri[keep]
dup = tri[rng.choice(len(tri), 40, replace=False)] + np.array([0.01, 0.0, 0.0])
tri = np.concatenate([tri, dup])
pts = tri.reshape(-1, 3); cells = np.arange(len(pts)).reshape(-1, 3)
meshio.write("models/bracket-dirty.stl", meshio.Mesh(pts, [("triangle", cells)]), binary=True)
print(len(t), "triangles in,", len(cells), "out")
