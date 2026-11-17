# box-twosteps: two analysis steps

box-setup's tension in two steps: Pulled moved 0.005 mm, then 0.01 mm.
Written whole by GridForge (`gf-study deck tension.json ...box.step --size
5 --order 2`), solved by ccx 2.23 on 2026-11-17 with no warning. Its
`tension.sta` holds a row per step, total time 1 then 2: the adapter's
progress must say half way, then done (test_calculix_adapter).
