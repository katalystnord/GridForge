"""One mesher, one model, one size: writes the mesh and prints the time.

    meshers.py MESHER MODEL H ORDER OUT THREADS

Each mesher is run out of the box: the same largest element size H and
otherwise its own defaults. Linear (ORDER 1) or quadratic (ORDER 2).
"""
import json
import os
import sys
import time

import numpy as np

here = os.path.dirname(os.path.abspath(__file__))


def gmsh_run(model, h, order, out, threads, algorithm, allhex=False):
    import gmsh
    gmsh.initialize()
    gmsh.option.setNumber("General.Terminal", 0)
    gmsh.option.setNumber("General.NumThreads", threads)
    t0 = time.perf_counter()
    gmsh.model.occ.importShapes(os.path.join(here, "models", model + ".step"))
    gmsh.model.occ.synchronize()
    gmsh.option.setNumber("Mesh.MeshSizeMax", h)
    gmsh.option.setNumber("Mesh.Algorithm3D", algorithm)
    gmsh.option.setNumber("Mesh.ElementOrder", order)
    if allhex:
        gmsh.option.setNumber("Mesh.SubdivisionAlgorithm", 2)
    gmsh.model.mesh.generate(3)
    elapsed = time.perf_counter() - t0
    gmsh.write(out)
    gmsh.finalize()
    return elapsed


def netgen_run(model, h, order, out, threads):
    from netgen.occ import OCCGeometry
    from pyngcore import SetNumThreads, TaskManager
    SetNumThreads(threads)
    t0 = time.perf_counter()
    with TaskManager():
        geo = OCCGeometry(os.path.join(here, "models", model + ".step"))
        mesh = geo.GenerateMesh(maxh=h)
        if order == 2:
            mesh.SecondOrder()
    elapsed = time.perf_counter() - t0
    write_netgen(mesh, out)
    return elapsed


def write_netgen(mesh, out):
    import meshio
    p = np.array([list(v.p) for v in mesh.Points()])
    tets = []
    for el in mesh.Elements3D():
        tets.append([v.nr - 1 for v in el.points])
    tets = np.array(tets)
    if tets.shape[1] == 10:
        # Netgen's edges: 01 02 03 12 13 23; VTK's (meshio's): 01 12 20 03 13 23.
        cells = [("tetra10", tets[:, [0, 1, 2, 3, 4, 7, 5, 6, 8, 9]])]
    else:
        cells = [("tetra", tets)]
    meshio.write(out, meshio.Mesh(p, cells))


def ftetwild_run(model, h, order, out, threads, stl=None):
    import pytetwild
    import pyvista as pv
    if order != 1:
        raise SystemExit("fTetWild makes linear tetrahedra only")
    t0 = time.perf_counter()
    surface = pv.read(stl or os.path.join(here, "models", model + ".stl")).clean().triangulate()
    grid = pytetwild.tetrahedralize_pv(surface, edge_length_abs=h, num_threads=threads)
    elapsed = time.perf_counter() - t0
    grid.save(out)
    return elapsed


def gmsh_stl_run(model, h, order, out, threads, stl):
    """Gmsh from a triangulated surface: classify, rebuild, fill."""
    import gmsh
    import math
    gmsh.initialize()
    gmsh.option.setNumber("General.Terminal", 0)
    gmsh.option.setNumber("General.NumThreads", threads)
    t0 = time.perf_counter()
    gmsh.merge(stl)
    gmsh.model.mesh.classifySurfaces(40 * math.pi / 180, True, True, math.pi)
    gmsh.model.mesh.createGeometry()
    s = gmsh.model.getEntities(2)
    l = gmsh.model.geo.addSurfaceLoop([e[1] for e in s])
    gmsh.model.geo.addVolume([l])
    gmsh.model.geo.synchronize()
    gmsh.option.setNumber("Mesh.MeshSizeMax", h)
    gmsh.option.setNumber("Mesh.ElementOrder", order)
    gmsh.model.mesh.generate(3)
    elapsed = time.perf_counter() - t0
    gmsh.write(out)
    gmsh.finalize()
    return elapsed


def netgen_stl_run(model, h, order, out, threads, stl):
    from netgen.stl import STLGeometry
    from pyngcore import SetNumThreads, TaskManager
    SetNumThreads(threads)
    t0 = time.perf_counter()
    with TaskManager():
        mesh = STLGeometry(stl).GenerateMesh(maxh=h)
        if order == 2:
            mesh.SecondOrder()
    elapsed = time.perf_counter() - t0
    write_netgen(mesh, out)
    return elapsed


MESHERS = {
    "gmsh-delaunay": lambda *a: gmsh_run(*a, algorithm=1),
    "gmsh-hxt": lambda *a: gmsh_run(*a, algorithm=10),
    "gmsh-allhex": lambda *a: gmsh_run(*a, algorithm=1, allhex=True),
    "netgen": netgen_run,
    "ftetwild": ftetwild_run,
}
FROM_STL = {"gmsh-stl": gmsh_stl_run, "netgen-stl": netgen_stl_run,
            "ftetwild-stl": lambda m, h, o, out, t, stl: ftetwild_run(m, h, o, out, t, stl)}

if __name__ == "__main__":
    mesher, model, h, order, out, threads = sys.argv[1:7]
    args = (model, float(h), int(order), out, int(threads))
    if mesher in FROM_STL:
        elapsed = FROM_STL[mesher](*args, sys.argv[7])
    else:
        elapsed = MESHERS[mesher](*args)
    print(json.dumps({"seconds": elapsed}))
