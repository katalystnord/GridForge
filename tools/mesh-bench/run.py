"""Runs every mesher on every model, twice, and evaluates each result.
Appends one JSON line per run to results.jsonl."""
import json, os, subprocess, sys, time

here = os.path.dirname(os.path.abspath(__file__))
py = os.path.join(here, ".venv", "bin", "python")
models = json.load(open(os.path.join(here, "models", "models.json")))
H = {"dogbone": 2.0, "bracket": 3.0, "thinwall": 1.5, "twoparts": 3.0, "duct": 2.0}
RUNS = []
for model in H:
    for mesher in ("gmsh-delaunay", "gmsh-hxt", "netgen"):
        for order in (1, 2):
            RUNS.append((mesher, model, order))
    RUNS.append(("gmsh-allhex", model, 1))
    RUNS.append(("ftetwild", model, 1))
only = sys.argv[1:]
with open(os.path.join(here, "results.jsonl"), "a") as log:
    for mesher, model, order in RUNS:
        if only and mesher not in only:
            continue
        for attempt in (1, 2):
            ext = "msh" if mesher.startswith("gmsh") else "vtu"
            out = os.path.join(here, "out", f"{model}-{mesher}-{order}-{attempt}.{ext}")
            rec = {"mesher": mesher, "model": model, "order": order, "attempt": attempt, "h": H[model]}
            t0 = time.time()
            try:
                r = subprocess.run([py, "meshers.py", mesher, model, str(H[model]), str(order), out, "8"],
                                   cwd=here, capture_output=True, text=True, timeout=900)
                if r.returncode != 0:
                    rec["failed"] = (r.stderr.strip().splitlines() or ["?"])[-1][:300]
                else:
                    rec.update(json.loads(r.stdout.strip().splitlines()[-1]))
                    e = subprocess.run([py, "evaluate.py", out, str(models[model]["volume"])],
                                       cwd=here, capture_output=True, text=True, timeout=900)
                    rec.update(json.loads(e.stdout) if e.returncode == 0 else {"eval_failed": e.stderr[-300:]})
            except subprocess.TimeoutExpired:
                rec["failed"] = "timeout (900 s)"
            rec["wall"] = time.time() - t0
            log.write(json.dumps(rec) + "\n"); log.flush()
            print(mesher, model, order, attempt, rec.get("failed") or f"{rec.get('seconds', 0):.2f}s", flush=True)
