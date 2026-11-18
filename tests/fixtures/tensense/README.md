# TenSense's analysed curves

`Zx3.vtp`: BAM's S355 tensile test Zx3 as TenSense 0.3.0 analysed it
(profile iso-6892-1-2019, S0 120.6 mm2, extensometer gauge 50 mm), written
with `ts-analyse --vtp` (TenSense's exports spec, section 3). The data:
Schilling, M., Glaubitz, S., Matzak, K., Rehmer, B., Skrotzki, B. (2022),
*Full dataset of several mechanical tests on an S355 steel sheet as
reference data for digital representations* (1.0.0), Zenodo,
doi:10.5281/zenodo.6778336, CC BY 4.0; `01_primary_data/Zx3.csv` of its
`tensile_test.zip` (SHA-256 b7dae010...e580, checked 2026-11-18). The
specimen: DIN 50125 form E 6 x 20 x 60, in the rolling direction (BAM's
protocol). The extensometer was taken off at about 3.9 % strain, so the
curve ends there (1906 samples).

What is known without GridForge's reader (read from the file's text with
Python, 2026-11-18): 1906 points; strain from -0.0002301 to 0.0390524;
force from 205.36 N to 53164.14 N, the last its largest; at a strain of
0.01, first reached at sample 720 (from 0), 44652.37 N between its
samples.
