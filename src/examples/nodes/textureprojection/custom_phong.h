/**
 * @file custom_phong.h
 * @brief 带纹理投影绑定的自定义 phong ShaderSet（加载 data/shaders 下的 GLSL）
 *
 * 移植自 vsgExamples/examples/nodes/vsgtextureprojection/custom_phong.h。
 * 在标准 phong 管线上增加一个自定义描述符集（set 0）：
 *   textureCount（uint）、texGenMatrices（mat4 数组，动态更新）、
 *   projectedTextures（texture2DArray）——片元着色器用这些矩阵做投影式采样。
 * 原版命名空间 custom，此处移入 vsglab::examples::custom。
 *
 * @author hxxcxx
 * @date 2026-11-16
 */
#pragma once

#include <vsg/io/Options.h>
#include <vsg/utils/ShaderSet.h>

namespace vsglab::examples::custom {

extern vsg::ref_ptr<vsg::ShaderSet> phong_ShaderSet(vsg::ref_ptr<const vsg::Options> options);

}  // namespace vsglab::examples::custom
