"""The duct with Netgen's wall layers, written as Gmsh 2 for gmshToFoam by
our own writer: Netgen 6.2.2608's OpenFOAM exporter crashed ("double free
or corruption") leaving empty files, and its Gmsh 2 exporter skips the
layers' quadrilateral faces. Netgen orders corners the other way from
Gmsh; each cell is turned by its signed volume, and the count is printed."""
import numpy as np
from netgen.occ import OCCGeometry
from netgen.meshing import BoundaryLayerParameters
geo = OCCGeometry("models/duct.step"); shape = geo.shape
for f in shape.faces:
    c = f.center
    f.name = "inlet" if abs(c.x) < 1e-3 else "outlet" if abs(c.y - 120) < 1e-3 else "wall"
bl = BoundaryLayerParameters(boundary="wall", thickness=[0.1 * 1.2 ** k for k in range(5)], new_material="layers")
mesh = OCCGeometry(shape).GenerateMesh(maxh=1.6, boundary_layers=[bl])
names = mesh.GetRegionNames(dim=2)
walls = sum(1 for el in mesh.Elements2D() if names[el.index - 1] == "wall")
p = np.array([list(v.p) for v in mesh.Points()])
cells, turned, prisms = [], 0, 0
for el in mesh.Elements3D():
    v = [x.nr - 1 for x in el.vertices]
    s = np.dot(np.cross(p[v[1]] - p[v[0]], p[v[2]] - p[v[0]]), p[v[3]] - p[v[0]])
    if len(v) == 4:
        if s < 0: v = [v[0], v[2], v[1], v[3]]; turned += 1
        cells.append((4, v))
    else:
        if s < 0: v = [v[0], v[2], v[1], v[3], v[5], v[4]]; turned += 1
        cells.append((6, v)); prisms += 1
with open("cfd/netgen-bl.msh", "w") as f:
    f.write("$MeshFormat\n2.2 0 8\n$EndMeshFormat\n$Nodes\n%d\n" % len(p))
    for i, x in enumerate(p, 1): f.write(f"{i} {x[0]:.12g} {x[1]:.12g} {x[2]:.12g}\n")
    f.write("$EndNodes\n$Elements\n%d\n" % len(cells))
    for i, (t, v) in enumerate(cells, 1): f.write(f"{i} {t} 2 1 1 " + " ".join(str(k + 1) for k in v) + "\n")
    f.write("$EndElements\n")
print(len(cells), "cells,", prisms, "prisms on", walls, "wall triangles,", turned, "turned")
