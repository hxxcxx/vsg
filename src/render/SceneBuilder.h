#pragma once

/// render 层：中性 MeshData -> VSG 场景节点。

#include "core/MeshData.h"

#include <vsg/all.h>

namespace facet::render {

/// 把网格包装成可渲染节点：StateGroup(Phong 管线 + 材质) + VertexIndexDraw。
/// 返回空 ref 表示网格为空。
vsg::ref_ptr<vsg::Node> buildMeshNode(const MeshData& mesh);

} // namespace facet::render
