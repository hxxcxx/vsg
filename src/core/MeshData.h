#pragma once

/// core 层：kernel 与 render 之间的中性数据边界。
/// 这里不允许出现 OCCT 或 VSG 的头文件。

#include <cstdint>
#include <vector>

namespace facet {

struct Vec3f
{
    float x = 0.f, y = 0.f, z = 0.f;
};

/// 一个 TopoDS_Face 网格化后在索引数组中的区段。
/// faceId 由 kernel 侧按遍历顺序分配，供将来做面级拾取/高亮。
struct FaceRange
{
    uint32_t faceId = 0;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

/// 三角网格。坐标已归一化：包围盒中心在原点、最长边 = 2。
/// 原始毫米尺寸通过 sourceScale 换算（1 显示单位 = sourceScale mm）。
struct MeshData
{
    std::vector<Vec3f> positions;   // 顶点，与 normals 一一对应
    std::vector<Vec3f> normals;     // 顶点法线（归一化）
    std::vector<uint32_t> indices;  // 三角形索引列表，每 3 个为一组
    std::vector<FaceRange> faceRanges;
    double sourceScaleMm = 1.0;     // 归一化时除以的尺度（mm / 显示单位）

    uint32_t triangleCount() const
    {
        return static_cast<uint32_t>(indices.size() / 3);
    }
};

} // namespace facet
