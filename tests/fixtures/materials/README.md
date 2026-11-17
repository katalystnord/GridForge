# Material cards for GridForge's tests

| file | from | known answer |
|---|---|---|
| `Steel-Generic.FCMat` | FreeCAD 1.1.4's own card (its Flatpak, `Mod/Material/Resources/Materials/Standard/Metal/Steel/`), by Juergen Riegel, CC-BY-3.0, copied unchanged | IsotropicLinearElastic: E 200000 MPa, nu 0.3; Density 7900 kg/m^3 |
| `tensense-ro.FCMat` | written by TenSense 0.3.0 (its FreeCAD check, `build-ninja/freecad-check/ro.FCMat`), copied unchanged | LinearElastic: E 199.9 GPa; no Poisson's ratio, no density (a tensile test measures neither) |
| `tensense-ro-plastic.inp` | written by TenSense 0.3.0 with `tensense-ro.FCMat`, from the same curve, copied unchanged | a `*PLASTIC` block, true stress against true plastic strain, MPa: Rp0.2 301.05504 MPa at 0, hardening to 666.63175 MPa at 0.28429565 |
| `hand-units.FCMat` | by hand | E 70 GPa, nu 0.33, density 2.7 g/cm^3 (in the file's comments in every unit) |
| `odd-unit.FCMat` | by hand | E in psi: refused, not converted |
| `poisson-unit.FCMat` | by hand | Poisson's ratio "0.3 MPa": refused |
| `broken.FCMat` | by hand | not YAML: refused |

Steel-Generic.FCMat is Juergen Riegel's work for FreeCAD, licensed
CC-BY-3.0 (its `General.License`); it is here to test reading, unchanged.
