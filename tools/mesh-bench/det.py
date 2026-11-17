"""Gmsh's repeatability on the bracket at 1.5 mm: one thread twice, and
eight threads twice with Mesh.Reproducible on. (Netgen's: meshers.py
run with 1 and 8 threads, hashed by evaluate.py.)"""
import json, os, subprocess, time
os.makedirs("out/det", exist_ok=True)
CODE = '''
import sys, gmsh
out, th, rep, alg = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
gmsh.initialize(); gmsh.option.setNumber("General.Terminal", 0)
gmsh.option.setNumber("General.NumThreads", th); gmsh.option.setNumber("Mesh.Reproducible", rep)
gmsh.model.occ.importShapes("models/bracket.step"); gmsh.model.occ.synchronize()
gmsh.option.setNumber("Mesh.MeshSizeMax", 1.5); gmsh.option.setNumber("Mesh.Algorithm3D", alg)
gmsh.model.mesh.generate(3); gmsh.write(out); gmsh.finalize()
'''
for alg, name in ((1, "delaunay"), (10, "hxt")):
    for th, rep in ((1, 0), (1, 0), (8, 1), (8, 1)):
        out = f"out/det/{name}-{th}-{rep}-{time.time_ns()}.msh"
        subprocess.run([".venv/bin/python", "-c", CODE, out, str(th), str(rep), str(alg)], check=True)
        d = json.loads(subprocess.run([".venv/bin/python", "evaluate.py", out, "60816.975593585215"], capture_output=True, text=True).stdout)
        print(name, "threads", th, "reproducible", rep, d["hash"], d["cells"], flush=True)
