# DRAFT for VTK's issue tracker: VTKHDF drops field data on collection blocks

**Status:** drafted 2026-11-16; not to be filed for now (David,
2026-11-17). Kept so it is ready if that changes. Searched before
drafting: no matching issue found; the nearest is
[vtk#19030](https://gitlab.kitware.com/vtk/vtk/-/issues/19030) (field data
on image data).

**Why it matters to GridForge:** our `.vtkhdf` results carry their
provenance (solver and version, source file, what was derived) as field
data on every block. ParaView 6.2 does not show it.

---

**Title:** vtkHDFReader drops the field data of the blocks of a
vtkPartitionedDataSetCollection

**Summary.** Field data set on the blocks of a
vtkPartitionedDataSetCollection is written by vtkHDFWriter (it is in the
file, under `/VTKHDF/Block0/FieldData`), but vtkHDFReader returns the
blocks without it. The same grid written on its own keeps its field data
through the round trip.

**Version.** VTK 9.7.1, as shipped in ParaView 6.2.0 (Flathub
`org.paraview.ParaView`), Linux x86_64. Not checked on master.

**To reproduce** (`vtkhdf-collection-field-data.py`, run with pvpython or
any Python with VTK 9.7):

```
VTK 9.7.1
plain grid, block field data read back: ['note', 'step']
collection, block field data read back: []
```

The file written for the collection holds:

```
G /VTKHDF/Block0
G /VTKHDF/Block0/FieldData
D VTKHDF/Block0/FieldData/note [1]
D VTKHDF/Block0/FieldData/step [1]
```

**Expected.** Each block read back with its `note` and `step` arrays, as a
plain grid is.

**Also seen.** In ParaView 6.2.0, a temporal collection written by VTK 9.5's
vtkHDFWriter (per-step `FieldDataOffsets` and `FieldDataSizes` present) shows
only the reader's own `Time` array in the block field data. VTK 9.5's
vtkHDFReader reads the same file's block field data back.
