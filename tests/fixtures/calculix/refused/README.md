# Refused .frd files

Made by hand from `../bar-tension/bar.frd` (2026-11-16), to check that a
broken file is refused with its line rather than read wrongly.

- `truncated.frd`: the first 20 lines; the file ends inside the node block.
- `unknown-element.frd`: element 1's type changed from 1 (he8) to 99,
  which is no element type in the cgx manual's table (line 35).
