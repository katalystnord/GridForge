"""The duct's surface for the OpenFOAM meshers: one STL with three named
solids, inlet (x = 0), outlet (y = 120) and wall (everything else)."""
import os
import gmsh
here = os.path.dirname(os.path.abspath(__file__))
gmsh.initialize(); gmsh.option.setNumber("General.Terminal", 0)
gmsh.model.occ.importShapes(os.path.join(here, "models", "duct.step"))
gmsh.model.occ.synchronize()
e = 1e-3
inlet = [t for _, t in gmsh.model.getEntitiesInBoundingBox(-e, -20, -20, e, 20, 20, 2)]
outlet = [t for _, t in gmsh.model.getEntitiesInBoundingBox(80, 120 - e, -20, 120, 120 + e, 20, 2)]
walls = [t for _, t in gmsh.model.getEntities(2) if t not in inlet + outlet]
print("inlet", inlet, "outlet", outlet, "walls", len(walls))
gmsh.option.setNumber("Mesh.MeshSizeMax", 0.8)
gmsh.model.mesh.generate(2)
with open(os.path.join(here, "cfd", "duct.stl"), "w") as f:
    for name, tags in (("inlet", inlet), ("outlet", outlet), ("wall", walls)):
        f.write(f"solid {name}\n")
        for t in tags:
            types, _, nodes = gmsh.model.mesh.getElements(2, t)
            for ty, nn in zip(types, nodes):
                for k in range(0, len(nn), 3):
                    p = [gmsh.model.mesh.getNode(int(n))[0] for n in nn[k:k + 3]]
                    f.write("facet normal 0 0 0\n outer loop\n")
                    for x in p:
                        f.write(f"  vertex {x[0]:.9g} {x[1]:.9g} {x[2]:.9g}\n")
                    f.write(" endloop\nendfacet\n")
        f.write(f"endsolid {name}\n")
gmsh.finalize()
