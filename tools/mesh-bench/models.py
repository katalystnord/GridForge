"""The benchmark's geometry, made with Gmsh's OpenCASCADE kernel: STEP for
the CAD-based meshers, STL for the surface-based ones, and each model's
exact volume (OCC's mass properties) as the reference."""
import json, math, os, sys
import gmsh
import numpy as np

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "models")
occ = None

def start(name):
    gmsh.clear(); gmsh.model.add(name)

def save(name, notes):
    occ.synchronize()
    vols = gmsh.model.getEntities(3)
    volume = sum(occ.getMass(3, t) for _, t in vols)
    # The outside: faces bounding one volume only. (Here the parts of
    # twoparts share their contact face; the STEP will hold it twice.)
    count = {}
    for v in vols:
        for _, s in gmsh.model.getBoundary([v], oriented=False):
            count[abs(s)] = count.get(abs(s), 0) + 1
    outer = sum(occ.getMass(2, s) for s, c in count.items() if c == 1)
    gmsh.write(os.path.join(out, name + ".step"))
    # A fine STL of the boundary for the surface-based meshers.
    gmsh.option.setNumber("Mesh.MeshSizeMax", 1.0)
    gmsh.option.setNumber("Mesh.MeshSizeMin", 0.2)
    gmsh.model.mesh.generate(2)
    gmsh.write(os.path.join(out, name + ".stl"))
    gmsh.model.mesh.clear()
    info[name] = {"volume": volume, "outer_area": outer, "volumes": len(vols), "notes": notes}

def dogbone():
    start("dogbone")
    L, Wg, Wn, Ln, R, T = 150.0, 20.0, 10.0, 60.0, 30.0, 4.0
    dy = (Wg - Wn) / 2
    # Each transition is an arc of radius R, tangent to the narrow edge,
    # meeting the grip's edge after dx along x.
    dx = math.sqrt(R ** 2 - (R - dy) ** 2)
    x0, x1 = (L - Ln) / 2, (L + Ln) / 2
    def P(x, y): return occ.addPoint(x, y, 0)
    # Outline, counter-clockwise, upper half mirrored.
    xa, xb = x0 - dx, x1 + dx
    p = [P(0, -Wg/2), P(xa, -Wg/2), P(x0, -Wn/2), P(x1, -Wn/2), P(xb, -Wg/2), P(L, -Wg/2),
         P(L, Wg/2), P(xb, Wg/2), P(x1, Wn/2), P(x0, Wn/2), P(xa, Wg/2), P(0, Wg/2)]
    c = [P(x0, -Wn/2 - R), P(x1, -Wn/2 - R), P(x1, Wn/2 + R), P(x0, Wn/2 + R)]
    l = [occ.addLine(p[0], p[1]), occ.addCircleArc(p[1], c[0], p[2]), occ.addLine(p[2], p[3]),
         occ.addCircleArc(p[3], c[1], p[4]), occ.addLine(p[4], p[5]), occ.addLine(p[5], p[6]),
         occ.addLine(p[6], p[7]), occ.addCircleArc(p[7], c[2], p[8]), occ.addLine(p[8], p[9]),
         occ.addCircleArc(p[9], c[3], p[10]), occ.addLine(p[10], p[11]), occ.addLine(p[11], p[0])]
    s = occ.addPlaneSurface([occ.addCurveLoop(l)])
    occ.extrude([(2, s)], 0, 0, T)
    save("dogbone", "150 x 20 x 4 mm, narrow part 60 x 10, R30 transitions")

def bracket():
    start("bracket")
    t = 8.0
    base = occ.addBox(0, 0, 0, 80, 60, t)
    wall = occ.addBox(0, 0, 0, t, 60, 50)
    rib = occ.addWedge(t, 25, t, 30, 10, 30)
    body, _ = occ.fuse([(3, base)], [(3, wall), (3, rib)])
    occ.synchronize()
    # Fillet the inside corner between base and wall.
    edges = [t_ for d, t_ in gmsh.model.getEntitiesInBoundingBox(t - 0.1, -0.1, t - 0.1, t + 0.1, 60.1, t + 0.1, 1)]
    vol = body[0][1]
    if edges:
        body = occ.fillet([vol], edges, [5.0])
        vol = body[0][1]
    holes = [occ.addCylinder(55, 15, -1, 0, 0, t + 2, 5), occ.addCylinder(55, 45, -1, 0, 0, t + 2, 5),
             occ.addCylinder(-1, 15, 35, t + 2, 0, 0, 5), occ.addCylinder(-1, 45, 35, t + 2, 0, 0, 5)]
    occ.cut([(3, vol)], [(3, h) for h in holes])
    save("bracket", "L bracket 80 x 60 x 50, 8 mm thick, R5 inner fillet, a rib, four 10 mm holes")

def thinwall():
    start("thinwall")
    outer = occ.addBox(0, 0, 0, 100, 60, 40)
    inner = occ.addBox(1.5, 1.5, 1.5, 97, 57, 40)
    occ.cut([(3, outer)], [(3, inner)])
    save("thinwall", "open box 100 x 60 x 40, walls 1.5 mm")

def twoparts():
    start("twoparts")
    a = occ.addBox(0, 0, 0, 40, 40, 20)
    b = occ.addCylinder(20, 20, 20, 0, 0, 30, 12)
    occ.fragment([(3, a)], [(3, b)])
    save("twoparts", "block 40 x 40 x 20 and a cylinder R12 x 30 standing on it: two volumes "
                     "sharing a face; the STEP holds the contact disk once per solid")

def duct():
    start("duct")
    R, Rb = 10.0, 40.0
    # Straight inlet along x, 90 degree bend, straight outlet along y.
    d = occ.addDisk(0, 0, 0, R, R, zAxis=[1, 0, 0])
    occ.extrude([(3 - 1, d)], 60, 0, 0)
    occ.synchronize()
    d2 = occ.addDisk(60, 0, 0, R, R, zAxis=[1, 0, 0])
    occ.revolve([(2, d2)], 60, Rb, 0, 0, 0, 1, math.pi / 2)
    d3 = occ.addDisk(60 + Rb, Rb, 0, R, R, zAxis=[0, 1, 0])
    occ.extrude([(2, d3)], 0, 80, 0)
    occ.synchronize()
    vols = gmsh.model.getEntities(3)
    fused, _ = occ.fuse([vols[0]], vols[1:])
    obstacle = occ.addCylinder(30, 0, -12, 0, 0, 24, 3)
    occ.cut(fused, [(3, obstacle)])
    save("duct", "pipe D20: 60 straight, 90 degree bend R40, 80 straight; a D6 rod across the inlet")

if __name__ == "__main__":
    os.makedirs(out, exist_ok=True)
    gmsh.initialize(); gmsh.option.setNumber("General.Terminal", 0)
    occ = gmsh.model.occ
    info = {}
    for f in (dogbone, bracket, thinwall, twoparts, duct):
        f()
    gmsh.finalize()
    json.dump(info, open(os.path.join(out, "models.json"), "w"), indent=1)
    print(json.dumps(info, indent=1))
