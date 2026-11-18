// Writes openbox.brep: the faces of a 10 mm cube, all but its last (the
// top, z = 10), taken as the shell of a solid; a surface that does not
// close, as a broken export can leave it. Netgen given it freed memory
// twice and ended the program (2026-11-18). As .brep, since OpenCASCADE's
// STEP reader drops a solid whose shell is open. Built and run by make.sh,
// once; the file is committed.
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>

int main()
{
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
    BRep_Builder b;
    TopoDS_Shell shell;
    b.MakeShell(shell);
    int n = 0;
    for (TopExp_Explorer e(box, TopAbs_FACE); e.More(); e.Next())
        if (n++ != 5)
            b.Add(shell, e.Current());
    TopoDS_Solid solid;
    b.MakeSolid(solid);
    b.Add(solid, shell);
    return BRepTools::Write(solid, "openbox.brep") ? 0 : 1;
}
