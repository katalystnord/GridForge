# Bar in tension, with named sets

The bar of `../bar-tension`, its sets written in each way the named-set
reader must follow: `nodes.inp` included, `NSET=` on `*NODE` and `ELSET=`
on `*ELEMENT`, one set over two `*ELEMENT` blocks, `GENERATE` with and
without a step, a set made of other sets plus a node it already holds, a
set continued over two lines, keywords and names in mixed case.

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, with
the command in `../make.sh`; `ccx.log`, `bar.sta` and `bar.cvg` kept.

## The known answer

ccx ran the deck, so it accepts every form used; the displacements are
bar-tension's, line for line, so it read the sets as meant (the boundary
on `x0` held the set `X0`, `FIXED` held `Fixed`: names ignore case).

The sets, by hand from the deck:

| Set | Kind | Ids |
|---|---|---|
| Nall | nodes | 1 to 20 (from `nodes.inp`) |
| Bar | elements | 1, 2, 3, 4 (two blocks) |
| Odd | elements | 1, 3 (`GENERATE` 1, 3, 2) |
| Top | elements | 4 |
| Fixed | nodes | 1, 2, 3, 4 (`GENERATE` 1, 4) |
| Pulled | nodes | 17, 18, 19, 20 |
| Ends | nodes | 1, 2, 3, 4, 17, 18, 19, 20 (Fixed, Pulled, 1) |
| X0 | nodes | 1, 4, 5, 8, 9, 12, 13, 16, 17, 20 |
| Y0 | nodes | 1, 2, 5, 6, 9, 10, 13, 14, 17, 18 (two lines) |
