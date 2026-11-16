/**
 * @file texture_projection_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsgtextureprojection
 *
 * 纹理投影：无人机（Builder 搭的箱体+四旋翼）沿圆形路径巡航，把若干图片像
 * 投影仪一样投到下方 1000 个随机几何体（箱/球/胶囊）上。
 * 核心：自定义 ShaderSet 增加一个描述符集——textureCount + texGenMatrices
 * （mat4 数组，每帧乘以视图逆矩阵后动态更新）+ projectedTextures（texture2DArray，
 * 多张图片叠成层）。附带硬阴影（HardShadows）与阴影参数调节。
 *
 * 与原版的差异：
 * - 去掉了地球在线瓦片分支（--bing-maps/--rm/--osm/--earth）：本机离线不可达，
 *   始终走平地分支（requiresBase = true，无 EllipsoidModel/Perspective），
 *   在线瓦片留待 vsgtiledatabase。
 * - 投影图片由两个文件参数提供（原版为命令行参数列表，可多张同尺寸叠层；
 *   第二张尺寸不符会按原版逻辑跳过并提示）。默认取 data/textures 下的图片。
 * - 去掉了 -t/--st/-f（基准；平均帧率改在 stop() 打印）、-p（相机动画）、
 *   -o（写场景）、--rh/--orh（ResourceHints 读写）、--drone（外部无人机模型）、
 *   --direction/--location/--scale（保持默认值）。
 *
 * @author hxxcxx
 * @date 2026-11-16
 */

#include "examples/nodes/textureprojection/custom_phong.h"
#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <vector>

namespace vsglab::examples {
namespace {

using framework::Apply;
using framework::BoolParam;
using framework::DoubleParam;
using framework::FileParam;
using framework::IntParam;
using framework::ParamSet;
using framework::WindowContext;

vsg::ref_ptr<vsg::Node> createLargeTestScene(vsg::ref_ptr<vsg::Options> options, vsg::Path textureFile = {},
                                             bool requiresBase = true, bool insertCullNode = true) {
    auto builder = vsg::Builder::create();
    builder->options = options;

    auto scene = vsg::Group::create();

    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;

    geomInfo.cullNode = insertCullNode;

    if (textureFile)
        stateInfo.image = vsg::read_cast<vsg::Data>(textureFile, options);
    vsg::box bounds(vsg::vec3(-500.0f, -500.0f, 0.0f), vsg::vec3(500.0f, 500.0f, 20.0f));

    uint32_t numBoxes = 400;
    uint32_t numSpheres = 300;
    uint32_t numCapsules = 300;

    vsg::vec3 size = bounds.max - bounds.min;
    auto assignRandomGeometryInfo = [&]() {
        vsg::vec3 offset(size.x * float(std::rand()) / float(RAND_MAX), size.y * float(std::rand()) / float(RAND_MAX),
                         size.z * float(std::rand()) / float(RAND_MAX));
        geomInfo.position = bounds.min + offset;
        geomInfo.dx.set(10.0f, 0.0f, 0.0f);
        geomInfo.dy.set(0.0f, 10.0f, 0.0f);
        geomInfo.dz.set(0.0f, 0.0f, 10.0f);

        geomInfo.color.set(float(std::rand()) / float(RAND_MAX), float(std::rand()) / float(RAND_MAX),
                           float(std::rand()) / float(RAND_MAX), 1.0f);
    };

    for (uint32_t bi = 0; bi < numBoxes; ++bi) {
        assignRandomGeometryInfo();
        auto model = builder->createBox(geomInfo, stateInfo);
        scene->addChild(model);
    }

    for (uint32_t bi = 0; bi < numSpheres; ++bi) {
        assignRandomGeometryInfo();
        auto model = builder->createSphere(geomInfo, stateInfo);
        scene->addChild(model);
    }

    for (uint32_t bi = 0; bi < numCapsules; ++bi) {
        assignRandomGeometryInfo();
        auto model = builder->createCapsule(geomInfo, stateInfo);
        scene->addChild(model);
    }

    if (requiresBase) {
        float diameter = static_cast<float>(vsg::length(bounds.max - bounds.min));
        geomInfo.position.set(static_cast<float>((bounds.min.x + bounds.max.x) * 0.5),
                              static_cast<float>((bounds.min.y + bounds.max.y) * 0.5),
                              static_cast<float>(bounds.min.z));
        geomInfo.dx.set(diameter, 0.0f, 0.0f);
        geomInfo.dy.set(0.0f, diameter, 0.0f);
        geomInfo.dz.set(0.0f, 0.0f, 1.0f);
        geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);

        stateInfo.two_sided = true;

        scene->addChild(builder->createQuad(geomInfo, stateInfo));
    }

    return scene;
}

vsg::ref_ptr<vsg::Node> createDroneModel(vsg::ref_ptr<vsg::Options> options, float droneSize) {
    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    vsg::Builder builder;
    builder.options = options;

    auto droneGroup = vsg::Group::create();

    geomInfo.dx.set(droneSize * 0.5f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, droneSize, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, droneSize * 0.2f);

    // body
    droneGroup->addChild(builder.createBox(geomInfo, stateInfo));

    geomInfo.dx.set(droneSize * 0.5f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, droneSize * 0.5f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, droneSize * 0.05f);
    geomInfo.color.set(1.0f, 0.8f, 0.4f, 1.0f);

    // rotors
    geomInfo.position.set(-droneSize * 0.5f, -droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(droneSize * 0.5f, -droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(droneSize * 0.5f, droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    geomInfo.position.set(-droneSize * 0.5f, droneSize * 0.5f, 0.0f);
    droneGroup->addChild(builder.createDisk(geomInfo, stateInfo));

    return droneGroup;
}

vsg::ref_ptr<vsg::Animation> createAnimationPath(vsg::ref_ptr<vsg::MatrixTransform> transform, double flight_scale,
                                                 double period) {
    auto path = vsg::Animation::create();
    path->mode = vsg::Animation::REPEAT;

    auto transformSampler = vsg::TransformSampler::create();
    auto keyframes = transformSampler->keyframes = vsg::TransformKeyframes::create();
    transformSampler->object = transform;
    path->samplers.push_back(transformSampler);

    vsg::dvec3 scale(1.0, 1.0, 1.0);
    double delta = period / 100.0;
    for (double time = 0.0; time < period; time += delta) {
        double theta = 2 * vsg::PI * (time / period);
        vsg::dvec3 position(flight_scale * sin(theta), flight_scale * cos(theta), 0.0);
        vsg::dquat rotation(-0.5 * vsg::PI - theta, vsg::dvec3(0.0, 0.0, 1.0));
        keyframes->add(time, position, rotation, scale);
    }
    return path;
}

class TextureProjectionExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        image_1_ = params.addFile("Projected image 1", "textures/VSGlogo.png", "Images (*.png *.jpg *.jpeg *.bmp)",
                                  { "textures/VSGlogo.png", "textures/wood.png" }, Apply::Restart);
        image_2_ = params.addFile("Projected image 2", "", "Images (*.png *.jpg *.jpeg *.bmp)",
                                  { "", "textures/wood.png" }, Apply::Restart);
        scene_texture_ = params.addFile("Scene texture (-i)", "", "Images (*.png *.jpg *.jpeg *.bmp)",
                                        { "", "textures/terrain.tif" }, Apply::Restart);
        num_shadow_maps_ = params.addInt("Shadow maps per light (--sm)", 1, 0, 4, Apply::Restart);
        max_shadow_distance_ =
                params.addDouble("Max shadow distance (--sd)", 2000.0, 10.0, 10000.0, 10.0, Apply::Restart);
        shadow_map_bias_ = params.addDouble("Shadow map bias (--sb)", 0.005, 0.0, 0.1, 0.001, Apply::Restart);
        lambda_ = params.addDouble("Shadow lambda (--lambda)", 0.5, 0.0, 2.0, 0.05, Apply::Restart);
        near_far_ratio_ = params.addDouble("Near/far ratio (--nf)", 0.001, 1e-6, 0.1, 0.001, Apply::Restart);
        drone_size_ = params.addDouble("Drone size", 10.0, 1.0, 100.0, 1.0, Apply::Restart);
        shader_debug_ = params.addBool("Shadow map debug (--shader-debug)", false, Apply::Restart);
        depth_clamp_ = params.addBool("Depth clamp (--dc)", false, Apply::Restart);
        cull_nodes_ = params.addBool("Cull nodes (--cull)", false, Apply::Restart);
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        auto options = vsg::Options::create(*ctx.options());

        auto deviceFeatures = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
        deviceFeatures->get().samplerAnisotropy = VK_TRUE;

        bool shaderDebug = shader_debug_->value();
        bool depthClamp = depth_clamp_->value();
        if (depthClamp) {
            std::cout << "Enabled depth clamp." << std::endl;
            deviceFeatures->get().depthClamp = VK_TRUE;
        }

        auto insertCullNode = cull_nodes_->value();
        vsg::dvec3 direction(0.0, 0.0, -1.0);
        vsg::dvec3 location(0.0, 0.0, 100.0);
        double viewingDistance = 1.0;

        auto textureFile = vsg::Path(scene_texture_->value());

        auto numShadowMapsPerLight = static_cast<uint32_t>(num_shadow_maps_->value());

        auto resourceHints = vsg::ResourceHints::create();

        // replace phong shader only for simplicity of example - vsg::TileDatabase and vsg::Builder use phong ShaderSet
        vsg::ref_ptr<vsg::ShaderSet> shaderSet = custom::phong_ShaderSet(options);

        options->shaderSets["phong"] = shaderSet;
        if (!shaderSet)
            throw vsg::Exception{ "No vsg::ShaderSet to process." };

        auto shaderHints = shaderSet->defaultShaderHints = vsg::ShaderCompileSettings::create();

        if (depthClamp || shaderDebug) {
            if (shaderDebug) {
                shaderHints->defines.insert("SHADOWMAP_DEBUG");
            }

            if (depthClamp) {
                auto rasterizationState = vsg::RasterizationState::create();
                rasterizationState->depthClampEnable = VK_TRUE;
                shaderSet->defaultGraphicsPipelineStates.push_back(rasterizationState);
            }

            // clear prebuilt variants
            shaderSet->variants.clear();
        }

        std::vector<vsg::ref_ptr<vsg::Data>> images;
        vsg::ref_ptr<vsg::Data> firstImage;
        uint32_t depth = 0;
        for (FileParam* param : { image_1_, image_2_ }) {
            vsg::Path filename = param->value();
            if (filename.empty())
                continue;

            if (auto data = vsg::read_cast<vsg::Data>(filename, options)) {
                data->properties.format = vsg::uNorm_to_sRGB(data->properties.format);
                if (data->properties.format == VK_FORMAT_R8G8B8A8_SRGB) {
                    if (!firstImage) {
                        firstImage = data;
                        depth += data->depth();
                        images.push_back(data);
                    } else if (firstImage->width() == data->width() && firstImage->height() == data->height()) {
                        depth += data->depth();
                        images.push_back(data);
                    } else {
                        std::cout << "Image file : " << filename
                                  << " loaded, but does not match required dimensions for first image." << std::endl;
                    }
                } else {
                    std::cout << "Image file : " << filename
                              << " loaded, but does not match required VK_FORMAT_R8G8B8A8_SRGB format." << std::endl;
                }
            } else {
                std::cout << "Image file : " << filename << " not loaded." << std::endl;
            }
        }

        if (images.empty())
            throw vsg::Exception{ "No images to project, please specify an image file." };

        // set up the state at the top of the scene graph that will be inherited
        auto stateGroup = vsg::StateGroup::create();

        auto textureCount = vsg::uintValue::create(depth);
        auto countDescriptor = vsg::DescriptorBuffer::create(textureCount);
        auto texgenDescriptor = vsg::DescriptorBuffer::create();
        auto textureDescriptor = vsg::DescriptorImage::create();

        {
            auto& countDescriptorBinding = shaderSet->getDescriptorBinding("textureCount");
            auto& texGenMatricesDescriptorBinding = shaderSet->getDescriptorBinding("texGenMatrices");
            auto& projectedTexturesDescriptorBinding = shaderSet->getDescriptorBinding("projectedTextures");
            //
            // place the FogValue DescriptorSet at the root of the scene graph
            // and tell the loader via vsg::Options::inheritedState that it doesn't need to assign
            // it to subgraphs that loader will create
            //
            auto layout = shaderSet->createPipelineLayout({}, { 0, 2 });

            uint32_t vds_set = 1;
            stateGroup->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

            uint32_t cm_set = 0;

            countDescriptor->dstBinding = countDescriptorBinding.binding;
            texgenDescriptor->dstBinding = texGenMatricesDescriptorBinding.binding;
            textureDescriptor->dstBinding = projectedTexturesDescriptorBinding.binding;

            auto cm_dsl = shaderSet->createDescriptorSetLayout({}, cm_set);
            auto cm_ds = vsg::DescriptorSet::create(
                    cm_dsl, vsg::Descriptors{ countDescriptor, texgenDescriptor, textureDescriptor });

            auto cm_bds = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, cm_ds);
            stateGroup->add(cm_bds);

            vsg::info("Added state to inherit ");
            options->inheritedState = stateGroup->stateCommands;
        }

        bool requiresBase = true;

        //
        // set up the drone subgraph
        //
        auto droneModel = createDroneModel(options, static_cast<float>(drone_size_->value()));

        auto droneLocation = vsg::MatrixTransform::create();   // transform into world coordinate frame
        auto droneTransform = vsg::MatrixTransform::create();  // local position of the drone within this droneLocation
        droneLocation->addChild(droneTransform);
        droneTransform->addChild(droneModel);

        //
        // set up the main scene graph
        //
        vsg::ref_ptr<vsg::Node> scene = createLargeTestScene(options, textureFile, requiresBase, insertCullNode);

        auto sampler = vsg::Sampler::create();
        auto texgenMatrices = vsg::mat4Array::create(depth);
        texgenMatrices->properties.dataVariance = vsg::DYNAMIC_DATA;

        auto texture2DArray = vsg::ubvec4Array3D::create(firstImage->width(), firstImage->height(), depth,
                                                         vsg::ubvec4(255, 255, 255, 255),
                                                         vsg::Data::Properties{ VK_FORMAT_R8G8B8A8_SRGB });
        texture2DArray->properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

        texgenDescriptor->bufferInfoList.push_back(vsg::BufferInfo::create(texgenMatrices));
        textureDescriptor->imageInfoList.push_back(vsg::ImageInfo::create(sampler, texture2DArray));

        double flight_scale = 400.0;
        double animationPeriod = 60.0;

        // create the path for the drone to follow, and to use for setting the texgen matrices to position the textures
        auto dronePath = createAnimationPath(droneTransform, flight_scale, animationPeriod);

        // copy loaded images to single texture2DArray
        uint32_t layer = 0;
        for (auto& data : images) {
            std::memcpy(texture2DArray->dataPointer(texture2DArray->index(0, 0, layer)), data->dataPointer(),
                        data->dataSize());
            layer += data->depth();
        }

        if (stateGroup) {
            // if setup place the StateGroup at the root of the scene graph
            stateGroup->addChild(scene);
            scene = stateGroup;
        }

        // compute the bounds of the scene graph to help position camera
        auto bounds = vsg::visit<vsg::ComputeBounds>(scene).bounds;
        viewingDistance = vsg::length(bounds.max - bounds.min) * 2.0;

        // 平地分支（原版另有地球在线瓦片分支，离线未移植）
        {
            vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;

            auto group = vsg::Group::create();
            group->addChild(scene);
            group->addChild(droneLocation);

            droneLocation->matrix = vsg::translate(location);

            scene = group;

            // set up the camera
            lookAt_ = vsg::LookAt::create(centre + vsg::dvec3(0.0, -viewingDistance, 0.0), centre,
                                          vsg::dvec3(0.0, 0.0, 1.0));
        }

        // set up the texgen matrices
        auto imageMatrices = vsg::dmat4Array::create(depth);
        double aspectRatio =
                (static_cast<double>(texture2DArray->width()) / static_cast<double>(texture2DArray->height()));
        dronePath->start(0.0);
        for (size_t i = 0; i < depth; ++i) {
            // drone has Y forward, X to the right, Z up, so without any further rotation the projected texture
            // will poject down the negative Z axis, which in this setup is straight down towards the earth.
            double time = dronePath->maxTime() * (double(i) / double(depth));
            dronePath->update(time);
            auto mv = vsg::inverse(droneLocation->matrix * droneTransform->matrix);
            auto p = vsg::perspective(vsg::radians(45.0), aspectRatio, 1.0, 100.0);
            imageMatrices->set(i, p * mv);
        }
        dronePath->stop(0.0);

        auto group = vsg::Group::create();
        group->addChild(scene);

        // set up lighting
        {
            auto directionalLight = vsg::DirectionalLight::create();
            directionalLight->name = "directional";
            directionalLight->color.set(1.0, 1.0, 1.0);
            directionalLight->intensity = 0.98f;
            directionalLight->direction = direction;
            if (numShadowMapsPerLight > 0) {
                directionalLight->shadowSettings = vsg::HardShadows::create(numShadowMapsPerLight);
            }
            group->addChild(directionalLight);

            auto ambientLight = vsg::AmbientLight::create();
            ambientLight->name = "ambient";
            ambientLight->color.set(1.0, 1.0, 1.0);
            ambientLight->intensity = 0.02f;
            group->addChild(ambientLight);
        }

        scene = group;

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create windows." };

        viewer->addWindow(window);

        auto perspective = vsg::Perspective::create(
                30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
                near_far_ratio_->value() * viewingDistance, viewingDistance * 1.3);

        auto camera = vsg::Camera::create(perspective, lookAt_, vsg::ViewportState::create(window->extent2D()));

        // add the camera and scene graph to View
        auto view = vsg::View::create();
        view->camera = camera;
        view->viewDependentState->maxShadowDistance = max_shadow_distance_->value();
        view->viewDependentState->shadowMapBias = shadow_map_bias_->value();
        view->viewDependentState->lambda = lambda_->value();
        view->addChild(scene);

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理
        viewer->addEventHandler(vsg::Trackball::create(camera));

        auto renderGraph = vsg::RenderGraph::create(window, view);
        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile(resourceHints);

        viewer->animationManager->play(dronePath);

        camera_ = camera;
        texgen_matrices_ = texgenMatrices;
        image_matrices_ = imageMatrices;
        depth_ = depth;
        start_time_ = vsg::clock::now();
        ctx.viewer = viewer;
    }

    void frame(WindowContext& ctx) override {
        ctx.viewer->handleEvents();
        ctx.viewer->update();

        // update the texgen matrices：投影矩阵 × 无人机位姿 × 视图逆矩阵，动态写回 UBO
        vsg::dmat4 inverse_viewMatrix = vsg::inverse(camera_->viewMatrix->transform());
        for (size_t i = 0; i < depth_; ++i) {
            vsg::dmat4 texgenProjView = image_matrices_->at(i);
            vsg::dmat4 texgenTM = texgenProjView * inverse_viewMatrix;
            texgen_matrices_->set(i, vsg::mat4(texgenTM));
        }
        texgen_matrices_->dirty();

        ctx.viewer->recordAndSubmit();
        ctx.viewer->present();
    }

    void stop(WindowContext& ctx) override {
        if (auto frameStamp = ctx.viewer ? ctx.viewer->getFrameStamp() : nullptr) {
            double duration =
                    std::chrono::duration<double, std::chrono::seconds::period>(vsg::clock::now() - start_time_)
                            .count();
            if (frameStamp->frameCount > 0 && duration > 0.0)
                std::cout << "Average frame rate = " << (static_cast<double>(frameStamp->frameCount) / duration)
                          << std::endl;
        }
    }

private:
    FileParam* image_1_ = nullptr;
    FileParam* image_2_ = nullptr;
    FileParam* scene_texture_ = nullptr;
    IntParam* num_shadow_maps_ = nullptr;
    DoubleParam* max_shadow_distance_ = nullptr;
    DoubleParam* shadow_map_bias_ = nullptr;
    DoubleParam* lambda_ = nullptr;
    DoubleParam* near_far_ratio_ = nullptr;
    DoubleParam* drone_size_ = nullptr;
    BoolParam* shader_debug_ = nullptr;
    BoolParam* depth_clamp_ = nullptr;
    BoolParam* cull_nodes_ = nullptr;

    vsg::ref_ptr<vsg::Camera> camera_;
    vsg::ref_ptr<vsg::mat4Array> texgen_matrices_;
    vsg::ref_ptr<vsg::dmat4Array> image_matrices_;
    size_t depth_ = 0;
    vsg::ref_ptr<vsg::LookAt> lookAt_;
    vsg::time_point start_time_;
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(TextureProjectionExample, "vsgtextureprojection", "nodes",
                        "Texture projection: a Builder-made drone circles above 1000 random boxes/spheres/capsules, "
                        "projecting images onto the scene through a custom phong ShaderSet with a per-frame "
                        "texGenMatrices descriptor — plus hard shadows with tunable parameters. Earth online-tile "
                        "branch omitted (offline).");

}  // namespace vsglab::examples
