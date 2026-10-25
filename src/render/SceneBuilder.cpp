#include "render/SceneBuilder.h"

#include <vsg/lighting/Light.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/state/material.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ShaderSet.h>

#include <cstring>
#include <type_traits>

namespace facet::render {

vsg::ref_ptr<vsg::Node> buildMeshNode(const MeshData& mesh)
{
    if (mesh.positions.empty() || mesh.positions.size() != mesh.normals.size()
        || mesh.indices.empty())
        return {};

    auto vertices = vsg::vec3Array::create(mesh.positions.size());
    std::memcpy(vertices->data(), mesh.positions.data(), sizeof(vsg::vec3) * mesh.positions.size());

    auto normals = vsg::vec3Array::create(mesh.normals.size());
    std::memcpy(normals->data(), mesh.normals.data(), sizeof(vsg::vec3) * mesh.normals.size());

    auto indices = vsg::uintArray::create(mesh.indices.size());
    static_assert(std::is_same_v<vsg::uintArray::value_type, uint32_t>);
    std::memcpy(indices->data(), mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

    // Phong 管线：ShaderSet 内嵌 SPIR-V（VSG 编译期生成，无需运行时 glslang）
    auto shaderSet = vsg::createPhongShaderSet();
    auto configurator = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    if (const auto& materialBinding = shaderSet->getDescriptorBinding("material"))
    {
        vsg::ref_ptr<vsg::Data> mat = materialBinding.data;
        if (!mat) mat = vsg::PhongMaterialValue::create();
        configurator->assignDescriptor("material", mat);
    }

    vsg::DataList arrays;
    configurator->assignArray(arrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    configurator->assignArray(arrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);

    // 对齐 Builder 行为：实例颜色属性（若着色器无条件读取 inColor，缺绑即纯黑）
    auto colors = vsg::vec4Array::create(1);
    (*colors)[0] = {1.f, 1.f, 1.f, 1.f};
    configurator->assignArray(arrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);
    configurator->init();

    auto stateGroup = vsg::StateGroup::create();
    configurator->copyTo(stateGroup);
    stateGroup->prototypeArrayState = configurator->getSuitableArrayState();

    auto draw = vsg::VertexIndexDraw::create();
    draw->assignArrays(arrays);
    draw->assignIndices(indices);
    draw->indexCount = static_cast<uint32_t>(mesh.indices.size());
    draw->instanceCount = 1;   // 默认 0：不设就是画 0 个实例
    stateGroup->addChild(draw);

    // VSG 不自动打光：必须显式挂头灯（环境光+方向光），否则光照为零、零件纯黑
    auto root = vsg::Group::create();
    root->addChild(vsg::createHeadlight());
    root->addChild(stateGroup);
    return root;
}

} // namespace facet::render
