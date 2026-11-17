"""results.jsonl as tables, one per model: the two runs of each
configuration side by side (repeatable: same hash)."""
import json, collections, os
here = os.path.dirname(os.path.abspath(__file__))
info = json.load(open(os.path.join(here, "models", "models.json")))
rows = collections.defaultdict(list)
for line in open(os.path.join(here, "results.jsonl")):
    r = json.loads(line)
    rows[(r["model"], r["mesher"], r["order"])].append(r)
for model in info:
    print(f"\n### {model} ({info[model]['notes']}; volume {info[model]['volume']:.1f} mm3)\n")
    print("| mesher | order | result | seconds | cells | mean shape | worst shape | min dihedral | volume error | bad quadratic | repeatable | boundary |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for (m, mesher, order), rs in sorted(rows.items()):
        if m != model: continue
        r = rs[0]
        if "failed" in r or "eval_failed" in r:
            print(f"| {mesher} | {order} | failed: {(r.get('failed') or r.get('eval_failed'))[:90]} |  |  |  |  |  |  |  |  |  |"); continue
        cells = ", ".join(f"{v} {k}" for k, v in r["cells"].items())
        q = r.get("tet_quality", {})
        ve = r.get("volume_curved_error", r["volume_error"])
        rep = "yes" if len(rs) > 1 and all(x.get("hash") == r["hash"] for x in rs) else "no"
        b = r.get("boundary_area")
        bnd = "" if b is None else f"{(b - info[model]['outer_area']) / info[model]['outer_area'] * 100:+.2f}%"
        sec = sum(x.get("seconds", 0) for x in rs) / len(rs)
        print(f"| {mesher} | {order} | ok | {sec:.2f} | {cells} | {q.get('mean', float('nan')):.3f} | {q.get('min', float('nan')):.3f} | "
              f"{r.get('tet_min_dihedral_deg', float('nan')):.1f} | {ve * 100:+.4f}% | {r.get('tet10_bad_jacobian', '')} | {rep} | {bnd} |")
