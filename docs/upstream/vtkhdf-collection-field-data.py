# Field data on the blocks of a vtkPartitionedDataSetCollection does not
# survive a VTKHDF round trip; on a plain vtkUnstructuredGrid it does.
import os, sys, tempfile
from vtkmodules.vtkCommonCore import vtkIntArray, vtkPoints, vtkStringArray, vtkVersion
from vtkmodules.vtkCommonDataModel import (vtkDataAssembly, vtkPartitionedDataSetCollection,
                                           vtkUnstructuredGrid)
from vtkmodules.vtkIOHDF import vtkHDFReader, vtkHDFWriter

def grid():
    g = vtkUnstructuredGrid()
    p = vtkPoints(); p.InsertNextPoint(0, 0, 0); g.SetPoints(p)
    s = vtkStringArray(); s.SetName("note"); s.InsertNextValue("hello"); g.GetFieldData().AddArray(s)
    i = vtkIntArray(); i.SetName("step"); i.InsertNextValue(1); g.GetFieldData().AddArray(i)
    return g

def names(fd):
    return [fd.GetAbstractArray(k).GetName() for k in range(fd.GetNumberOfArrays())]

def round_trip(data, path):
    w = vtkHDFWriter(); w.SetInputData(data); w.SetFileName(path); w.Write()
    r = vtkHDFReader(); r.SetFileName(path); r.Update()
    return r.GetOutputDataObject(0)

print("VTK", vtkVersion.GetVTKVersionFull())
d = tempfile.mkdtemp()
print("plain grid, block field data read back:", names(round_trip(grid(), os.path.join(d, "g.vtkhdf")).GetFieldData()))
c = vtkPartitionedDataSetCollection(); c.SetNumberOfPartitionedDataSets(1); c.SetPartition(0, 0, grid())
a = vtkDataAssembly(); a.AddDataSetIndex(a.AddNode("Model"), 0); c.SetDataAssembly(a)
out = round_trip(c, os.path.join(d, "c.vtkhdf"))
print("collection, block field data read back:", names(out.GetPartition(0, 0).GetFieldData()))
