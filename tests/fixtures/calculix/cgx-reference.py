#!/usr/bin/env python3
"""Writes <fixture>/cgx-reference.txt: the numbers cgx shows for a result.

For the last DISP and the last STRESS dataset in <fixture>/<job>.frd, cgx
(run without a window, -bg -b) prints every node's value of each entity:
the displacements D1 D2 D3, the stresses SXX ... SZX as ccx wrote them,
and what cgx derives from them: Mises, PS1, PS2, PS3. The test suite
compares GridForge's arrays with these, so it never needs cgx itself.

cgx prints values with six decimals ("node:18 v:59.809658"), so these
hold six decimals too. Run by hand when a fixture changes:

    python3 cgx-reference.py cantilever-te10/cantilever.frd ...
"""

import os
import re
import subprocess
import sys
import tempfile

# cgx's entity numbers in a STRESS dataset (cgx 2.21: ds <n> e <k>).
STRESS = [("SXX", 1), ("SYY", 2), ("SZZ", 3), ("SXY", 4), ("SYZ", 5), ("SZX", 6),
          ("Mises", 7), ("PS1", 11), ("PS2", 15), ("PS3", 19)]
DISP = [("D1", 1), ("D2", 2), ("D3", 3)]


def datasets(frd):
    """Dataset numbers as cgx counts them (one per 100C record), by name."""
    names = []
    with open(frd, "rb") as f:
        for line in f:
            if line.startswith(b"  100C"):
                names.append(None)
            elif line.startswith(b" -4") and names and names[-1] is None:
                names[-1] = line[5:13].decode().strip()
    return names


def cgx_version():
    out = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "calculix-cgx"],
                         capture_output=True, text=True)
    return out.stdout.strip() or "unknown"


def main(frd):
    frd = os.path.abspath(frd)
    names = datasets(frd)
    last = {n: len(names) - names[::-1].index(n) for n in ("DISP", "STRESS")}
    entities = [(last["DISP"], label, e) for label, e in DISP]
    entities += [(last["STRESS"], label, e) for label, e in STRESS]
    with tempfile.TemporaryDirectory() as tmp:
        # The node numbers, from cgx's own listing of its set "all".
        subprocess.run(["cgx", "-bg", "-b",
                        write(tmp, "list.fbd", [f"read {frd}", "send all lst", "quit"])],
                       cwd=tmp, capture_output=True, check=True)
        # The listing holds "# nodes" and then "# elements": nodes only.
        nodes, section = [], None
        with open(os.path.join(tmp, "all.lst")) as f:
            for line in f:
                if line.startswith("#"):
                    section = line[1:].strip()
                elif section == "nodes":
                    nodes += [int(t) for t in line.split()]
        # Each entity selected in turn, every node's value printed.
        script = [f"read {frd}"]
        for ds, _, e in entities:
            script.append(f"ds {ds} e {e}")
            script += [f"prnt n {n}" for n in nodes]
        script.append("quit")
        out = subprocess.run(["cgx", "-bg", "-b", write(tmp, "values.fbd", script)],
                             cwd=tmp, capture_output=True, text=True, check=True).stdout
    values = re.findall(r"node:(\d+) v:(\S+)", out)
    if len(values) != len(entities) * len(nodes):
        sys.exit(f"{frd}: cgx printed {len(values)} values, "
                 f"expected {len(entities) * len(nodes)}")
    table = {n: [] for n in nodes}
    for k, (node, v) in enumerate(values):
        expect = nodes[k % len(nodes)]
        if int(node) != expect:
            sys.exit(f"{frd}: value {k} is for node {node}, expected {expect}")
        table[expect].append(v)
    target = os.path.join(os.path.dirname(frd), "cgx-reference.txt")
    with open(target, "w") as f:
        f.write(f"# What cgx {cgx_version()} shows for {os.path.basename(frd)}: "
                f"DISP dataset {last['DISP']}, STRESS dataset {last['STRESS']}.\n")
        f.write("# Written by ../cgx-reference.py; Mises and PS1 to PS3 are cgx's own.\n")
        f.write("node " + " ".join(label for _, label, _ in entities) + "\n")
        for n in nodes:
            f.write(f"{n} " + " ".join(table[n]) + "\n")
    print(target, f"({len(nodes)} nodes)")


def write(tmp, name, lines):
    path = os.path.join(tmp, name)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return path


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        main(arg)
