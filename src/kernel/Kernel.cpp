#include "kernel/Kernel.h"

// OCCT 仅存在于本编译单元
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <array>
#include <cmath>

namespace facet {
namespace {

TopoDS_Shape makePlate(double L, double W, double T, double holeR, double filletR)
{
    TopoDS_Shape plate = BRepPrimAPI_MakeBox(L, W, T).Shape();

    gp_Ax2 holeAxis(gp_Pnt(L * 0.5, W * 0.5, -T), gp_Dir(0., 0., 1.));
    TopoDS_Shape hole = BRepPrimAPI_MakeCylinder(holeAxis, holeR, T * 3.).Shape();

    TopoDS_Shape cut = BRepAlgoAPI_Cut(plate, hole).Shape();

    // 全部棱倒圆角；失败则退回直角版本
    try
    {
        BRepFilletAPI_MakeFillet fillet(cut);
        for (TopExp_Explorer edge(cut, TopAbs_EDGE); edge.More(); edge.Next())
            fillet.Add(filletR, TopoDS::Edge(edge.Current()));
        TopoDS_Shape filleted = fillet.Shape();
        if (!filleted.IsNull()) return filleted;
    }
    catch (const Standard_Failure&) {}
    return cut;
}

MeshData tessellate(const TopoDS_Shape& shape, double linearDeflection, double angularDeflection)
{
    BRepMesh_IncrementalMesh mesher(shape, linearDeflection, false, angularDeflection);

    MeshData mesh;
    std::vector<Vec3f>& pos = mesh.positions;
    std::vector<Vec3f>& nrm = mesh.normals;

    uint32_t faceId = 0;
    for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next(), ++faceId)
    {
        const TopoDS_Face& face = TopoDS::Face(faceExp.Current());
        TopLoc_Location loc;
        const Handle(Poly_Triangulation)& tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull() || tri->NbTriangles() == 0 || tri->NbNodes() == 0) continue;

        const gp_Trsf& trsf = loc.Transformation();
        const bool reversed = (face.Orientation() == TopAbs_REVERSED);
        const bool identity = loc.IsIdentity();

        // 先收集本面三角形（注意面朝向翻转）
        std::vector<std::array<int, 3>> triangles;
        triangles.reserve(static_cast<size_t>(tri->NbTriangles()));
        for (int t = 1; t <= tri->NbTriangles(); ++t)
        {
            int a, b, c;
            tri->Triangle(t).Get(a, b, c);
            if (reversed) std::swap(b, c);
            triangles.push_back({a, b, c});
        }

        // OCCT 自带法线优先；否则用首个有效三角形做平面法线
        const bool hasNormals = tri->HasNormals();
        Vec3f flat{0.f, 0.f, 1.f};
        if (!hasNormals)
        {
            for (const auto& t3 : triangles)
            {
                gp_Pnt p1 = tri->Node(t3[0]), p2 = tri->Node(t3[1]), p3 = tri->Node(t3[2]);
                gp_Vec v1(p1, p2), v2(p1, p3), n = v1.Crossed(v2);
                if (n.Magnitude() > gp::Resolution())
                {
                    n.Normalize();
                    flat = {static_cast<float>(n.X()), static_cast<float>(n.Y()), static_cast<float>(n.Z())};
                    break;
                }
            }
        }

        const uint32_t base = static_cast<uint32_t>(pos.size());
        for (int i = 1; i <= tri->NbNodes(); ++i)
        {
            gp_Pnt p = tri->Node(i);
            if (!identity) p.Transform(trsf);
            pos.push_back({static_cast<float>(p.X()), static_cast<float>(p.Y()), static_cast<float>(p.Z())});

            if (hasNormals)
            {
                gp_Dir d = tri->Normal(i);
                if (!identity) d.Transform(trsf);
                if (reversed) d.Reverse();
                nrm.push_back({static_cast<float>(d.X()), static_cast<float>(d.Y()), static_cast<float>(d.Z())});
            }
            else
            {
                nrm.push_back(flat);
            }
        }

        mesh.faceRanges.push_back({faceId, static_cast<uint32_t>(mesh.indices.size()),
                                   static_cast<uint32_t>(triangles.size() * 3)});
        for (const auto& t3 : triangles)
        {
            mesh.indices.push_back(base + t3[0] - 1);
            mesh.indices.push_back(base + t3[1] - 1);
            mesh.indices.push_back(base + t3[2] - 1);
        }
    }

    // 归一化：中心移到原点，最长边缩放到 2
    if (pos.empty()) return {};

    Vec3f lo = pos.front(), hi = pos.front();
    for (const auto& p : pos)
    {
        lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
    }
    const double maxDim = std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z});
    if (maxDim <= 0.) return {};

    const double scale = maxDim * 0.5;   // mm / 显示单位
    const double cx = (lo.x + hi.x) * 0.5, cy = (lo.y + hi.y) * 0.5, cz = (lo.z + hi.z) * 0.5;
    for (auto& p : pos)
        p = {static_cast<float>((p.x - cx) / scale), static_cast<float>((p.y - cy) / scale),
             static_cast<float>((p.z - cz) / scale)};

    mesh.sourceScaleMm = scale;
    return mesh;
}

} // namespace

MeshData createDemoPlate(double lengthMm, double widthMm, double thicknessMm,
                         double holeRadiusMm, double filletRadiusMm)
{
    TopoDS_Shape shape = makePlate(lengthMm, widthMm, thicknessMm, holeRadiusMm, filletRadiusMm);
    // 0.1mm 线性偏差：demo 级精度，圆角处已足够光滑
    return tessellate(shape, 0.1, 0.4);
}

} // namespace facet
