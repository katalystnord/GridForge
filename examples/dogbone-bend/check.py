#!/usr/bin/env python3
# The bent dogbone against beam theory (README.md): the tip's deflection
# under the push, and the move's reaction against the push's stiffness.
#
#   python3 check.py job-push job-move
#
# Reads ccx's own job.frd in each job's work/ folder, not GridForge's
# converted results, so the check does not lean on the converter.

import math
import sys

E, T, L, F, MOVE = 199900.0, 4.0, 150.0, 10.0, 2.0  # MPa, mm, mm, N, mm


def width(x):
    # 20 mm grips, a 10 mm narrow part from x = 45 to 105, and R30 arcs
    # centred at (45, +-35) and (105, +-35) (dogbone.step's CIRCLEs).
    reach = math.sqrt(30**2 - 25**2)
    for c, lo, hi in ((45.0, 45.0 - reach, 45.0), (105.0, 105.0, 105.0 + reach)):
        if lo <= x <= hi:
            return 2 * (35 - math.sqrt(900 - (x - c) ** 2))
    return 10.0 if 45 < x < 105 else 20.0


def beam_tip(force):
    # Euler-Bernoulli, clamped at x = 0: tip = F/E * integral of (L-x)^2 / I(x),
    # I = b t^3 / 12, by the midpoint rule.
    n = 150000
    h = L / n
    s = sum((L - (i + 0.5) * h) ** 2 / width((i + 0.5) * h) for i in range(n)) * h
    return 12 * force / (E * T**3) * s


def frd(path):
    # The node block and each results block (" -4  DISP" ...), last step's
    # values winning: one step here.
    out, name = {}, None
    for line in open(path):
        if line.startswith("    2C"):
            name = "NODE"
            out[name] = {}
        elif line.startswith(" -4"):
            name = line.split()[1]
            out[name] = {}
        elif line.startswith(" -3"):
            name = None
        elif name and line.startswith(" -1"):
            s = line[13:].rstrip("\n")
            out[name][int(line[3:13])] = [float(s[i : i + 12]) for i in range(0, len(s), 12)]
    return out


def ends(job):
    d = frd(job + "/work/job.frd")
    tip = [n for n, p in d["NODE"].items() if abs(p[0] - L) < 1e-6]
    root = [n for n, p in d["NODE"].items() if abs(p[0]) < 1e-6]
    uz = sum(d["DISP"][n][2] for n in tip) / len(tip)
    rz = sum(d["FORC"][n][2] for n in root)
    return uz, rz


push, move = sys.argv[1:3]
expected = beam_tip(F)
uz, rz = ends(push)
print(f"push {F:g} N in -z: tip {uz:.5f} mm, beam theory {-expected:.5f} mm ({(-uz / expected - 1) * 100:+.2f} %); "
      f"clamp reaction {rz:.4f} N")
uz2, rz2 = ends(move)
print(f"move {MOVE:g} mm in -z: tip {uz2:.5f} mm; clamp reaction {rz2:.4f} N, "
      f"from the push's stiffness {F * MOVE / -uz:.4f} N")
ok = abs(-uz / expected - 1) < 0.01 and abs(rz - F) < 1e-3 and abs(rz2 - F * MOVE / -uz) < 1e-3 * rz2
print("agrees" if ok else "DOES NOT AGREE")
sys.exit(0 if ok else 1)
