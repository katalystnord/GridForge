"""The duct with 5 wall layers (first 0.1 mm, growth 1.2) and a 1.6 mm
core, by Gmsh (extrudeBoundaryLayer, after its tube example) and Netgen
(Mesh.BoundaryLayer); and plain Gmsh tetrahedra for polyDualMesh. Each
written where OpenFOAM's tools can take it.

    cfd_layers.py gmsh-bl | netgen-bl | gmsh-tet
"""
import json
import os
import sys
import time

import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
step = os.path.join(here, "models", "duct.step")
cfd = os.path.join(here, "cfd")
first, growth, layers, core = 0.1, 1.2, 5, 1.6
heights = list(np.cumsum([first * growth ** k for k in range(layers)]))


def walls_inlet_outlet(gmsh):
    e = 1e-3
    inlet = [t for _, t in gmsh.model.getEntitiesInBoundingBox(-e, -20, -20, e, 20, 20, 2)]
    outlet = [t for _, t in gmsh.model.getEntitiesInBoundingBox(80, 120 - e, -20, 120, 120 + e, 20, 2)]
    walls = [t for _, t in gmsh.model.getEntities(2) if t not in inlet + outlet]
    return inlet, outlet, walls


def gmsh_bl():
    import gmsh
    gmsh.initialize()
    gmsh.option.setNumber("General.Terminal", 0)
    gmsh.option.setNumber("General.NumThreads", 8)
    t0 = time.perf_counter()
    gmsh.model.occ.importShapes(step)
    gmsh.model.occ.synchronize()
    inlet, outlet, walls = walls_inlet_outlet(gmsh)
    # As the tube example: keep the walls only, extrude them inward.
    gmsh.model.occ.remove(gmsh.model.getEntities(3))
    gmsh.model.occ.remove([(2, t) for t in inlet + outlet])
    gmsh.model.occ.synchronize()
    gmsh.option.setNumber("Mesh.MeshSizeMax", core)
    gmsh.option.setNumber("Geometry.ExtrudeReturnLateralEntities", 0)
    # The example extrudes with negative heights to go inward.
    e = gmsh.model.geo.extrudeBoundaryLayer(gmsh.model.getEntities(2), [1] * layers,
                                            [-h for h in heights], True)
    top = [s for s in e if s[0] == 2]
    top_surf = [s[1] for s in top]
    gmsh.model.geo.synchronize()
    bnd = [c[1] for c in gmsh.model.getBoundary(top)]
    for loop in gmsh.model.geo.addCurveLoops(bnd):
        top_surf.append(gmsh.model.geo.addPlaneSurface([loop]))
    gmsh.model.geo.addVolume([gmsh.model.geo.addSurfaceLoop(top_surf)])
    gmsh.model.geo.synchronize()
    gmsh.model.mesh.generate(3)
    elapsed = time.perf_counter() - t0
    gmsh.option.setNumber("Mesh.MshFileVersion", 2.2)
    gmsh.option.setNumber("Mesh.SaveAll", 1)
    out = os.path.join(cfd, "gmsh-bl.msh")
    gmsh.write(out)
    gmsh.finalize()
    return elapsed, out


def gmsh_tet():
    import gmsh
    gmsh.initialize()
    gmsh.option.setNumber("General.Terminal", 0)
    gmsh.option.setNumber("General.NumThreads", 8)
    t0 = time.perf_counter()
    gmsh.model.occ.importShapes(step)
    gmsh.model.occ.synchronize()
    inlet, outlet, walls = walls_inlet_outlet(gmsh)
    for name, tags in (("inlet", inlet), ("outlet", outlet), ("wall", walls)):
        gmsh.model.setPhysicalName(2, gmsh.model.addPhysicalGroup(2, tags), name)
    gmsh.model.addPhysicalGroup(3, [t for _, t in gmsh.model.getEntities(3)])
    gmsh.option.setNumber("Mesh.MeshSizeMax", core)
    gmsh.model.mesh.generate(3)
    elapsed = time.perf_counter() - t0
    gmsh.option.setNumber("Mesh.MshFileVersion", 2.2)
    out = os.path.join(cfd, "gmsh-tet.msh")
    gmsh.write(out)
    gmsh.finalize()
    return elapsed, out


def netgen_bl():
    from netgen.occ import OCCGeometry
    from pyngcore import SetNumThreads, TaskManager
    SetNumThreads(8)
    t0 = time.perf_counter()
    geo = OCCGeometry(step)
    shape = geo.shape
    for f in shape.faces:
        c = f.center
        f.name = "inlet" if abs(c.x) < 1e-3 else "outlet" if abs(c.y - 120) < 1e-3 else "wall"
    with TaskManager():
        from netgen.meshing import BoundaryLayerParameters
        # Netgen 6.2.2608: layers are asked for in GenerateMesh. Thickness
        # per layer, not cumulative (the total is measured afterwards).
        bl = BoundaryLayerParameters(boundary="wall",
                                     thickness=[first * growth ** k for k in range(layers)],
                                     new_material="layers")
        mesh = OCCGeometry(shape).GenerateMesh(maxh=core, boundary_layers=[bl])
    elapsed = time.perf_counter() - t0
    case = os.path.join(cfd, "netgen-bl")
    os.makedirs(os.path.join(case, "system"), exist_ok=True)
    mesh.Export(case, "OpenFOAM 1.5+ Format")
    return elapsed, case


if __name__ == "__main__":
    which = sys.argv[1]
    try:
        elapsed, out = {"gmsh-bl": gmsh_bl, "netgen-bl": netgen_bl, "gmsh-tet": gmsh_tet}[which]()
        print(json.dumps({"seconds": elapsed, "out": out}))
    except Exception as ex:
        print(json.dumps({"failed": f"{type(ex).__name__}: {ex}"[:400]}))
