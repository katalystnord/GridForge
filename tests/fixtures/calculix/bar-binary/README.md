# Bar in tension, binary results

`bar.inp`: `../bar-tension/bar.inp` with `*NODE OUTPUT` and `*ELEMENT
OUTPUT` in place of `*NODE FILE` and `*EL FILE`, which makes ccx write
the `.frd` in binary (ccx manual).

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16.

## The layout, read from this file byte by byte

The record headers (1C, 1U, 2C, 3C, 1P, 100C, -4, -5, 9999) stay ASCII
lines. The records under them are binary, little-endian, and the block
has no -3 record after them. The format number at the end of each block
header (columns 74 and 75) says which:

| Block | Format | Each record |
|---|---|---|
| nodes (2C) | 3 | int32 id, then x, y, z as float64: 28 bytes |
| elements (3C) | 2 | int32 id, type, group, material, then the node ids as int32 |
| results (100C) | 2 | int32 node id, then each supplied component as float32 |

## The known answer

As `../bar-tension`. The values are the same results as the ASCII file's,
unrounded and in single precision: szz is 209.99159 where the ASCII
file writes 209.992, and every value agrees with the ASCII file's to
the ASCII file's five digits.
