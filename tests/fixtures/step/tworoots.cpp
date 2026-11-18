// Writes tworoots.step: two 10 mm cubes, apart (x 0 to 10 and 20 to 30),
// each its own root in the file, as an OpenCASCADE writer given one body
// at a time writes them. Built and run by make.sh, once; the file is
// committed.
#include <BRepPrimAPI_MakeBox.hxx>
#include <STEPControl_Writer.hxx>
#include <gp_Pnt.hxx>

int main()
{
    STEPControl_Writer w;
    w.Transfer(BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), 10, 10, 10).Shape(), STEPControl_AsIs);
    w.Transfer(BRepPrimAPI_MakeBox(gp_Pnt(20, 0, 0), 10, 10, 10).Shape(), STEPControl_AsIs);
    return w.Write("tworoots.step") == IFSelect_RetDone ? 0 : 1;
}
