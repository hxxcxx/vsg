/**
 * @file coordinate_frame_example.cpp
 * @brief 移植自 vsgExamples/examples/nodes/vsgcoordinateframe
 *
 * 天文尺度的精度教学：宇宙里放两个相距 1e9（或"最坏情形" 8.514e25）米的太阳系，
 * 各自带着自转/公转动画与多个观察点。核心对比——
 *   CoordinateFrame：以 long-double 精度的 origin 为局部原点，矩阵只存"局部小坐标"
 *   MatrixTransform（--mt）：整棵树的矩阵串进 4x4 double，远离原点时精度崩塌
 * 相机默认用 LookDirection（带 origin 的视图矩阵）配合 StellarManipulator：
 * 数字键 1..7 在各观察点间平滑飞行（"Numerical precision test" 按钮可打印
 * float/double/long double 在 1e26 处的精度，直观解释为什么要 CoordinateFrame）。
 *
 * 与原版的差异：
 * - 地球由在线瓦片数据库（--osm/--rm/--rme/--bing-maps）改为 Builder 生成的球体：
 *   本机离线，所有瓦片源均不可达；在线瓦片留待 vsgtiledatabase 示例处理。
 *   地球半径仍取 EllipsoidModel 的 WGS84 赤道半径，尺度教学不受影响。
 * - 因地球不再用瓦片，pbr ShaderSet 与 -t/-m/--rgb/--file-cache 未创建/移植。
 * - 去掉了 --dc/--depthClamp（设备特性支线）、-o（写场景）、--log-level、
 *   --viewpoint（初始观察点固定取第一个，与原版默认行为一致）。
 * - --wc（最坏距离预设）并入 "Distance preset" 下拉；--sr 星空半径并入距离推导。
 * - 交互模式（原版 -i）默认开启；原版默认的自动巡游仍可用 Mode 参数选回。
 * - creteSolarSystem 拼写修正为 createSolarSystem。
 *
 * @author hxxcxx
 * @date 2026-11-15
 */

#include "framework/example_registry.h"
#include "framework/window_example.h"

#include <vsg/all.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <thread>

namespace vsglab::examples {
namespace {

using framework::ActionParam;
using framework::Apply;
using framework::BoolParam;
using framework::ChoiceParam;
using framework::ColorParam;
using framework::DoubleParam;
using framework::IntParam;
using framework::ParamBase;
using framework::ParamSet;
using framework::WindowContext;

struct SolarSystemSettings {
    std::string name;
    vsg::ref_ptr<vsg::Builder> builder;
    vsg::ref_ptr<vsg::Options> options;
    double earth_to_sun_distance = 1.49e8;
    double sun_radius = 6.957e7;
    vsg::vec4 sun_color = { 1.0f, 1.0f, 0.5f, 1.0f };
    vsg::dvec3 position;
    vsg::dquat rotation;
    bool useCoordinateFrame = false;
    double windowAspectRatio = 1.25;
    double z_near = 1e2;
    double z_far = 1e15;

    vsg::ref_ptr<vsg::ShaderSet> flatShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> phongShaderSet;
};

vsg::ref_ptr<vsg::Node> createStarfield(double maxRadius, size_t numStars, vsg::ref_ptr<vsg::ShaderSet> shaderSet) {
    if (numStars == 0)
        return {};

    // set up the star positions
    auto vertices = vsg::vec3Array::create(numStars);
    auto colors = vsg::vec4Array::create(numStars);
    for (auto& v : *vertices) {
        v.x = static_cast<float>(2.0 * maxRadius *
                                 (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5));
        v.y = static_cast<float>(2.0 * maxRadius *
                                 (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5));
        v.z = static_cast<float>(2.0 * maxRadius *
                                 (static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX) - 0.5));
    }

    for (auto& c : *colors) {
        float brightness = static_cast<float>(std::rand() / static_cast<float>(RAND_MAX));
        c.set(brightness, brightness, brightness, 1.0f);
    }

    auto normals = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
    auto texcoords = vsg::vec2Value::create(vsg::vec2(1.0f, 1.0f));

    // configure the state and array layouts to assign to the scene graph
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shaderSet);

    graphicsPipelineConfig->shaderHints->defines.insert("VSG_POINT_SPRITE");

    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_INSTANCE, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, colors);

    // set up passing of material
    auto mat = vsg::PhongMaterialValue::create();
    graphicsPipelineConfig->assignDescriptor("material", mat);

    struct SetPipelineStates : public vsg::Visitor {
        void apply(vsg::Object& object) override { object.traverse(*this); }

        void apply(vsg::InputAssemblyState& ias) override { ias.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; }
    } sps;

    graphicsPipelineConfig->accept(sps);

    auto vertexDraw = vsg::VertexDraw::create();
    vertexDraw->assignArrays(vertexArrays);
    vertexDraw->vertexCount = static_cast<uint32_t>(numStars);
    vertexDraw->instanceCount = 1;
    vertexDraw->firstBinding = graphicsPipelineConfig->baseAttributeBinding;

    graphicsPipelineConfig->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    graphicsPipelineConfig->copyTo(stateGroup);

    stateGroup->addChild(vertexDraw);

    return stateGroup;
}

vsg::ref_ptr<vsg::Node> createSolarSystem(SolarSystemSettings& settings) {
    double day = 24.0 * 60.0 * 60.0;
    double year = 365.25 * day;
    double earth_radius = vsg::EllipsoidModel::create()->radiusEquator();

    // 地球：离线球体（原版为在线瓦片数据库，见文件头说明）
    settings.builder->shaderSet = settings.phongShaderSet;
    vsg::GeometryInfo earth_geom;
    float earth_size = static_cast<float>(2.0 * earth_radius);
    earth_geom.dx.set(earth_size, 0.0f, 0.0f);
    earth_geom.dy.set(0.0f, earth_size, 0.0f);
    earth_geom.dz.set(0.0f, 0.0f, earth_size);
    earth_geom.color.set(0.2f, 0.4f, 0.8f, 1.0f);
    vsg::StateInfo earth_state;
    earth_state.lighting = true;
    auto earth = settings.builder->createSphere(earth_geom, earth_state);

    auto earth_view = vsg::MatrixTransform::create();
    earth_view->setValue("viewpoint", settings.name + "_3_palace_view");
    earth_view->setObject("projection", vsg::Perspective::create(0.005, settings.windowAspectRatio, settings.z_near,
                                                                 settings.z_far));  // min FOV 0.005
    earth_view->matrix = vsg::rotate(vsg::radians(-51.315), 0.0, 1.0, 0.0) *
                         vsg::rotate(vsg::radians(89.915), 0.0, 0.0, 1.0) *
                         vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, earth_radius * 5.0);

    auto earth_rotation_about_axis = vsg::MatrixTransform::create();
    earth_rotation_about_axis->addChild(earth);
    earth_rotation_about_axis->addChild(earth_view);

    auto orbit_view = vsg::MatrixTransform::create();
    orbit_view->setValue("viewpoint", settings.name + "_2_orbit_view");
    orbit_view->setObject("projection",
                          vsg::Perspective::create(90, settings.windowAspectRatio, settings.z_near, settings.z_far));
    orbit_view->matrix = vsg::rotate(vsg::radians(45.0), 0.0, 0.0, 1.0) *
                         vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(0.0, 0.0, earth_radius * 5.0);

    auto earth_position_from_sun = vsg::MatrixTransform::create();
    earth_position_from_sun->addChild(earth_rotation_about_axis);
    earth_position_from_sun->addChild(orbit_view);
    earth_position_from_sun->matrix = vsg::translate(settings.earth_to_sun_distance, 0.0, 0.0);

    auto earth_orbit_transform = vsg::MatrixTransform::create();
    earth_orbit_transform->addChild(earth_position_from_sun);

    // animate the earths rotation around it's axis
    auto earth_keyframes = vsg::TransformKeyframes::create();
    earth_keyframes->add(0.0, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    earth_keyframes->add(day * 0.5, vsg::dvec3(0.0, 0.0, 0.0),
                         vsg::dquat(vsg::radians(180.0), vsg::dvec3(0.0, 0.0, 1.0)));
    earth_keyframes->add(day, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(360.0), vsg::dvec3(0.0, 0.0, 1.0)));

    auto earth_rotation_about_axisSampler = vsg::TransformSampler::create();
    earth_rotation_about_axisSampler->keyframes = earth_keyframes;
    earth_rotation_about_axisSampler->object = earth_rotation_about_axis;

    auto earth_animation = vsg::Animation::create();
    earth_animation->samplers.push_back(earth_rotation_about_axisSampler);

    // animate the earths rotation around the sun
    auto orbit_keyframes = vsg::TransformKeyframes::create();
    orbit_keyframes->add(0.0, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(0.0), vsg::dvec3(0.0, 0.0, 1.0)));
    orbit_keyframes->add(year * 0.5, vsg::dvec3(0.0, 0.0, 0.0),
                         vsg::dquat(vsg::radians(180.0), vsg::dvec3(0.0, 0.0, 1.0)));
    orbit_keyframes->add(year, vsg::dvec3(0.0, 0.0, 0.0), vsg::dquat(vsg::radians(360.0), vsg::dvec3(0.0, 0.0, 1.0)));

    auto orbit_transformSampler = vsg::TransformSampler::create();
    orbit_transformSampler->keyframes = orbit_keyframes;
    orbit_transformSampler->object = earth_orbit_transform;

    auto oribit_animation = vsg::Animation::create();
    oribit_animation->samplers.push_back(orbit_transformSampler);

    auto animationGroup = vsg::AnimationGroup::create();
    animationGroup->animations.push_back(earth_animation);
    animationGroup->animations.push_back(oribit_animation);

    // create sun

    settings.builder->shaderSet = settings.flatShaderSet;

    // vsg::Builder uses floats for sizing as it's intended for small local objects,
    // we'll ignore limitations for now as we won't be going close to sun's surface'
    float geometrySize = static_cast<float>(2.0 * settings.sun_radius);
    vsg::GeometryInfo geom;
    geom.dx.set(geometrySize, 0.0f, 0.0f);
    geom.dy.set(0.0f, geometrySize, 0.0f);
    geom.dz.set(0.0f, 0.0f, geometrySize);
    geom.color = settings.sun_color;

    vsg::StateInfo state;
    state.lighting = false;

    auto sun = settings.builder->createSphere(geom, state);

    auto sun_view = vsg::MatrixTransform::create();
    sun_view->setValue("viewpoint", settings.name + "_1_sun_view");
    sun_view->setObject("projection",
                        vsg::Perspective::create(60, settings.windowAspectRatio, settings.z_near, settings.z_far));
    sun_view->matrix = vsg::rotate(vsg::radians(70.0), 1.0, 0.0, 0.0) *
                       vsg::translate(0.0, 0.0, settings.earth_to_sun_distance * 3.0);

    auto light = vsg::PointLight::create();
    light->intensity = static_cast<float>(settings.earth_to_sun_distance * settings.earth_to_sun_distance);
    light->position.set(0.0f, 0.0f, 0.0f);
    light->color = settings.sun_color.rgb;

    if (settings.useCoordinateFrame) {
        auto solar_system = vsg::CoordinateFrame::create();

        solar_system->addChild(sun);
        solar_system->addChild(sun_view);
        solar_system->addChild(light);
        solar_system->addChild(earth_orbit_transform);
        solar_system->addChild(animationGroup);

        solar_system->name = settings.name;
        solar_system->origin = settings.position;
        solar_system->rotation = settings.rotation;

        return solar_system;
    } else {
        auto solar_system = vsg::MatrixTransform::create();

        solar_system->addChild(sun);
        solar_system->addChild(sun_view);
        solar_system->addChild(light);
        solar_system->addChild(earth_orbit_transform);
        solar_system->addChild(animationGroup);

        solar_system->matrix = vsg::translate(settings.position) * vsg::rotate(settings.rotation);

        return solar_system;
    }
}

class FindViewpoints : public vsg::Visitor {
public:
    std::multimap<std::string, vsg::RefObjectPath> viewpoints;
    vsg::ObjectPath objectPath;

    void apply(vsg::Object& object) {
        objectPath.push_back(&object);

        std::string name;
        if (object.getValue("viewpoint", name)) {
            viewpoints.emplace(name, vsg::RefObjectPath(objectPath.begin(), objectPath.end()));
        }

        object.traverse(*this);

        objectPath.pop_back();
    }
};

struct MyComputeTransform : public vsg::Visitor {
    vsg::ldvec3 origin;
    vsg::dmat4 matrix;

    vsg::ref_ptr<vsg::ProjectionMatrix> projection;

    void apply(vsg::Object& object) override {
        if (auto user_projection = object.getObject<vsg::ProjectionMatrix>("projection"))
            projection = user_projection;
    }

    void apply(vsg::Transform& transform) override {
        apply(static_cast<vsg::Object&>(transform));

        matrix = transform.transform(matrix);
    }

    void apply(vsg::CoordinateFrame& cf) override {
        apply(static_cast<vsg::Object&>(cf));

        origin = cf.origin;
        matrix = vsg::rotate(cf.rotation);
    }

    void apply(vsg::MatrixTransform& mt) override {
        apply(static_cast<vsg::Object&>(mt));

        matrix = matrix * mt.matrix;
    }

    void apply(vsg::Camera& camera) override {
        if (camera.projectionMatrix) {
            projection = camera.projectionMatrix;
        }
        if (camera.viewMatrix) {
            matrix = matrix * camera.viewMatrix->inverse();
        }
    }

    template <typename T>
    void apply(T& nodePath) {
        for (auto& node : nodePath)
            node->accept(*this);
    }
};

class StellarManipulator : public vsg::Inherit<vsg::Visitor, StellarManipulator> {
public:
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::ViewMatrix> viewMatrix;
    vsg::ref_ptr<vsg::ProjectionMatrix> projectionMatrix;
    double windowAspectRatio = 1.0;

    std::multimap<std::string, vsg::RefObjectPath> viewpoints;
    vsg::RefObjectPath currentFocus;

    double animationDuration = 5.0;
    vsg::time_point startTime;
    vsg::RefObjectPath startViewpoint;
    vsg::RefObjectPath targetViewpoint;

    StellarManipulator(vsg::ref_ptr<vsg::Camera> in_camera) : camera(in_camera) {
        viewMatrix = camera->viewMatrix;
        projectionMatrix = camera->projectionMatrix;

        auto viewport = camera->getViewport();
        windowAspectRatio = static_cast<double>(viewport.width) / static_cast<double>(viewport.height);
    }

    void apply(vsg::KeyPressEvent& keyPress) override {
        if (keyPress.keyBase >= vsg::KEY_1 && keyPress.keyBase <= vsg::KEY_9) {
            if (viewpoints.empty())
                return;

            uint16_t key = vsg::KEY_1;
            auto itr = viewpoints.begin();
            while (key < keyPress.keyBase && itr != viewpoints.end()) {
                ++itr;
                ++key;
            }

            if (itr == viewpoints.end())
                return;

            if (animationDuration <= 0.0 || currentFocus.empty()) {
                currentFocus = itr->second;
            } else {
                startTime = keyPress.time;
                startViewpoint = currentFocus;
                targetViewpoint = itr->second;
            }
        }
    }

    void apply(vsg::ConfigureWindowEvent& congfigureWindow) override {
        windowAspectRatio = static_cast<double>(congfigureWindow.width) / static_cast<double>(congfigureWindow.height);
        vsg::info("windowAspectRatio = ", windowAspectRatio);
    }

    void apply(vsg::FrameEvent& frame) override {
        using origin_value_type = decltype(MyComputeTransform::origin)::value_type;

        if (!targetViewpoint.empty() && !startViewpoint.empty()) {
            double timeSinceAnimationStart =
                    std::chrono::duration<double, std::chrono::seconds::period>(frame.time - startTime).count();

            if (timeSinceAnimationStart < animationDuration) {
                MyComputeTransform startTransform;
                startTransform.apply(startViewpoint);

                MyComputeTransform targetTransform;
                targetTransform.apply(targetViewpoint);

                vsg::dvec3 startTranslation, startScale, targetTranslation, targetScale;
                vsg::dquat startRotation, targetRotation;

                vsg::decompose(startTransform.matrix, startTranslation, startRotation, startScale);
                vsg::decompose(targetTransform.matrix, targetTranslation, targetRotation, targetScale);

                double tr = (timeSinceAnimationStart / animationDuration);
                double r = 1.0 - (1.0 + cos(tr * vsg::PI)) * 0.5;

                auto perspective = vsg::cast<vsg::Perspective>(projectionMatrix);
                auto startPerspective = vsg::cast<vsg::Perspective>(startTransform.projection);
                auto targetPerspective = vsg::cast<vsg::Perspective>(targetTransform.projection);
                if (perspective && startPerspective && targetPerspective) {
                    perspective->fieldOfViewY =
                            vsg::mix(startPerspective->fieldOfViewY, targetPerspective->fieldOfViewY, r);
                }

                if (auto lookDirection = vsg::cast<vsg::LookDirection>(viewMatrix)) {
                    lookDirection->origin =
                            vsg::mix(startTransform.origin, targetTransform.origin, static_cast<origin_value_type>(r));
                    lookDirection->position = vsg::mix(startTranslation, targetTranslation, r);
                    lookDirection->rotation = vsg::mix(startRotation, targetRotation, r);
                } else if (auto lookAt = vsg::cast<vsg::LookAt>(viewMatrix)) {
                    vsg::dvec3 position = vsg::mix(startTranslation, targetTranslation, r);
                    vsg::dquat rotation = vsg::mix(startRotation, targetRotation, r);

                    vsg::dvec3 lookVector(0.0, 0.0, -1.0);
                    vsg::dvec3 upVector(0.0, 1.0, 0.0);
                    double lookDistance = vsg::length(lookAt->center - lookAt->eye);

                    lookVector = rotation * lookVector;
                    upVector = rotation * upVector;

                    lookAt->origin =
                            vsg::mix(startTransform.origin, targetTransform.origin, static_cast<origin_value_type>(r));
                    lookAt->eye = position;
                    lookAt->center = position + lookVector * lookDistance;
                    lookAt->up = upVector;
                }

                return;
            } else {
                currentFocus = targetViewpoint;
            }
        }

        if (!currentFocus.empty()) {
            MyComputeTransform mct;
            mct.apply(currentFocus);

            viewMatrix->origin = mct.origin;
            if (auto lookDirection = vsg::cast<vsg::LookDirection>(viewMatrix)) {
                lookDirection->set(mct.matrix);
            } else if (auto lookAt = vsg::cast<vsg::LookAt>(viewMatrix)) {
                lookAt->set(mct.matrix);
            }

            auto perspective = vsg::cast<vsg::Perspective>(projectionMatrix);
            auto targetPerspective = vsg::cast<vsg::Perspective>(mct.projection.get());
            if (targetPerspective) {
                perspective->fieldOfViewY = targetPerspective->fieldOfViewY;
            }
            if (perspective)
                perspective->aspectRatio = windowAspectRatio;

            startViewpoint.clear();
            targetViewpoint.clear();
        }
    }
};

template <typename T>
T precision(T v) {
    T delta = v;
    while (v < (v + delta)) {
        delta /= static_cast<T>(2.0);
    }
    //
    while (v == (v + delta)) {
        delta *= static_cast<T>(1.001);
    }

    return delta;
}

template <typename T>
void print_bytes(std::ostream& out, T value) {
    out << value << " {\t";
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&value);
    for (size_t i = 0; i < sizeof(T); ++i) {
        out << static_cast<uint32_t>(*(ptr + i)) << "\t";
    }
    out << "}" << std::endl;
}

void numerical_test() {
    std::cout << "\nnative_long_double_bits() = " << vsg::native_long_double_bits() << std::endl;

    std::cout << "\nnumeric_limits<>" << std::endl;
    std::cout << "std::numeric_limits<float>::max() = " << std::numeric_limits<float>::max() << ", digits "
              << std::numeric_limits<float>::digits << ", digits10 " << std::numeric_limits<float>::digits10
              << ", sizeof<float> = " << sizeof(float) << std::endl;
    std::cout << "std::numeric_limits<double>::max() = " << std::numeric_limits<double>::max() << ", digits "
              << std::numeric_limits<double>::digits << ", digits10 " << std::numeric_limits<double>::digits10
              << ", sizeof<double> = " << sizeof(double) << std::endl;
    std::cout << "std::numeric_limits<long double>::max() = " << std::numeric_limits<long double>::max() << ", digits "
              << std::numeric_limits<long double>::digits << ", digits10 " << std::numeric_limits<long double>::digits10
              << ", sizeof<long double> = " << sizeof(long double) << std::endl;

    std::cout << "\nprecision around offset " << std::endl;
    std::cout << "    precision<float>(1e26) = " << precision<float>(1.0e26f) << std::endl;
    std::cout << "    precision<double>(1e26) = " << precision<double>(1.0e26) << std::endl;
    std::cout << "    precision<long double>(1e26) = " << precision<long double>(1.0e26) << std::endl;
}

class CoordinateFrameExample : public framework::WindowExample {
public:
    void declareParams(ParamSet& params) override {
        use_coordinate_frame_ = params.addBool("Use CoordinateFrame", true, Apply::Restart);
        view_matrix_ = params.addChoice("View matrix", { "LookDirection", "LookAt (--lookAt)" }, "LookDirection",
                                        Apply::Restart);
        distance_preset_ =
                params.addChoice("Distance preset", { "1e9", "Worst case 8.514e25 (--wc)" }, "1e9", Apply::Restart);
        mode_ = params.addChoice("Mode", { "Interactive keys 1..9 (-i)", "Auto tour" }, "Interactive keys 1..9 (-i)",
                                 Apply::Restart);
        play_ = params.addBool("Play orbit animations (--play)", false, Apply::Restart);
        speed_ = params.addDouble("Animation speed (--speed)", 1.0, 0.0, 10000000.0, 1000.0, Apply::Live);
        ambient_ = params.addDouble("Ambient light (--ambient)", 0.0, 0.0, 2.0, 0.05, Apply::Restart);
        num_stars_ = params.addInt("Starfield stars (--ns)", 100000, 0, 1000000, Apply::Restart);
        near_ = params.addDouble("z near (-n)", 1e3, 1.0, 1e8, 100.0, Apply::Restart);
        far_ = params.addDouble("z far (-f, 0 = auto)", 0.0, 0.0, 1e28, 1e24, Apply::Restart);
        clear_color_ = params.addColor("Clear color (--clear)", vsg::vec4(0.0f, 0.0f, 0.0f, 1.0f), Apply::Restart);
        numerical_ = params.addAction("Numerical precision test (--nt)");
    }

    void start(WindowContext& ctx) override {
        auto windowTraits = ctx.windowTraits();
        windowTraits->depthFormat = VK_FORMAT_D32_SFLOAT;
        auto options = ctx.options();

        SolarSystemSettings settings;
        settings.flatShaderSet = vsg::createFlatShadedShaderSet(options);
        settings.phongShaderSet = vsg::createPhongShaderSet(options);
        settings.windowAspectRatio =
                static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height);
        settings.options = options;
        settings.builder = vsg::Builder::create();
        settings.builder->options = options;
        settings.useCoordinateFrame = use_coordinate_frame_->value();

        const double distance_between_systems = (distance_preset_->value() == "1e9") ? 1.0e9 : 8.514e25;
        const double starfieldRadius = distance_between_systems * 4.0;

        settings.z_near = near_->value();
        settings.z_far = far_->value() > 0.0 ? far_->value() : starfieldRadius * 2.0;

        auto universe = vsg::Group::create();

        if (ambient_->value() > 0.0f) {
            auto ambientLight = vsg::AmbientLight::create();
            ambientLight->intensity = static_cast<float>(ambient_->value());
            universe->addChild(ambientLight);
        }

        // create starfield
        if (auto starfield = createStarfield(starfieldRadius, static_cast<size_t>(num_stars_->value()),
                                             settings.flatShaderSet)) {
            universe->addChild(starfield);
        }

        //
        // create solar system one
        //
        settings.name = "1. ";
        settings.sun_color.set(1.0f, 1.0f, 0.7f, 1.0f);
        settings.position.set(-distance_between_systems * 0.5, 0.0, 0.0);
        settings.rotation = {};
        auto solar_system_one = createSolarSystem(settings);

        settings.name = "2. ";
        settings.sun_color.set(1.0f, 0.8f, 0.7f, 1.0f);
        settings.position.set(distance_between_systems * 0.5, 0.0, 0.0);
        settings.rotation.set(vsg::radians(90.0), vsg::dvec3(1.0, 0.0, 0.0));
        auto solar_system_two = createSolarSystem(settings);

        auto universe_view = vsg::MatrixTransform::create();
        universe_view->setValue("viewpoint", "0. universe_view");
        universe_view->setObject("projection", vsg::Perspective::create(60, settings.windowAspectRatio, settings.z_near,
                                                                        settings.z_far));
        universe_view->matrix = vsg::rotate(vsg::radians(70.0), 1.0, 0.0, 0.0) *
                                vsg::translate(0.0, 0.0, distance_between_systems * 3.0);

        universe->addChild(solar_system_one);
        universe->addChild(solar_system_two);
        universe->addChild(universe_view);

        // get the viewpoints
        auto viewpoints = vsg::visit<FindViewpoints>(universe).viewpoints;
        for (auto& [name, objectPath] : viewpoints) {
            std::cout << "viewpoint [" << name << "] ";
            for (auto& obj : objectPath)
                std::cout << obj << " ";
            std::cout << std::endl;
        }

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();
        auto window = vsg::Window::create(windowTraits);
        if (!window)
            throw vsg::Exception{ "Could not create window." };

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        universe->accept(computeBounds);
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        vsg::dvec3 eye = center + vsg::dvec3(0.0, -radius * 3.5, 0.0);

        vsg::info("universe bounds computeBounds.bounds = ", computeBounds.bounds);

        // set up the camera
        vsg::ref_ptr<vsg::ViewMatrix> viewMatrix;
        if (view_matrix_->value() == "LookAt (--lookAt)") {
            auto lookAt = vsg::LookAt::create();
            lookAt->origin.set(0.0, 0.0, 0.0);
            lookAt->eye = eye;
            lookAt->center = center;
            lookAt->up.set(0.0, 0.0, 1.0);
            viewMatrix = lookAt;
        } else {
            auto lookDirection = vsg::LookDirection::create();
            lookDirection->origin.set(0.0, 0.0, 0.0);
            lookDirection->position = eye;
            viewMatrix = lookDirection;
        }

        settings.windowAspectRatio =
                static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height);

        auto perspective = vsg::Perspective::create(30.0, settings.windowAspectRatio, settings.z_near, settings.z_far);
        auto camera = vsg::Camera::create(perspective, viewMatrix, vsg::ViewportState::create(window->extent2D()));

        // 原版的 CloseHandler 不需要：窗口生命周期由宿主管理

        if (play_->value()) {
            animations_ = vsg::visit<vsg::FindAnimations>(universe).animations;
            for (auto& animation : animations_) {
                animation->speed = speed_->value();
                viewer->animationManager->play(animation);
            }
        }

        if (mode_->value() == "Interactive keys 1..9 (-i)") {
            // set up manipulator for interactively moving between viewpoints
            auto stellarManipulator = StellarManipulator::create(camera);
            stellarManipulator->viewpoints = viewpoints;
            viewer->addEventHandler(stellarManipulator);

            if (!viewpoints.empty()) {
                stellarManipulator->currentFocus = viewpoints.begin()->second;
                vsg::info("initial viewpoint : ", viewpoints.begin()->first);
            }
        } else {
            // set up camera animation to take you between each viewpoint
            auto cameraAnimationHandler = vsg::CameraAnimationHandler::create();
            cameraAnimationHandler->animation = vsg::Animation::create();
            cameraAnimationHandler->object = camera;

            auto cameraSampler = cameraAnimationHandler->cameraSampler = vsg::CameraSampler::create();
            cameraSampler->object = camera;
            cameraAnimationHandler->animation->samplers.push_back(cameraSampler);

            auto cameraKeyframes = cameraSampler->keyframes = vsg::CameraKeyframes::create();
            double time = 0.0;
            double time_pause = 3.0;
            double time_moving = 6.0;
            for (auto& [name, objectPath] : viewpoints) {
                cameraKeyframes->tracking.push_back(vsg::time_path{ time, objectPath });
                cameraKeyframes->tracking.push_back(vsg::time_path{ time + time_pause, objectPath });

                for (auto& object : objectPath) {
                    if (auto perspective = object->getRefObject<vsg::Perspective>("projection")) {
                        cameraKeyframes->fieldOfViews.push_back(vsg::time_double{ time, perspective->fieldOfViewY });
                        cameraKeyframes->fieldOfViews.push_back(
                                vsg::time_double{ time + time_pause, perspective->fieldOfViewY });
                    }
                }

                time += (time_pause + time_moving);
            }

            cameraAnimationHandler->play();

            viewer->addEventHandler(cameraAnimationHandler);
        }

        // 最后一个参数 false：不加头灯——场景里已有各自的 PointLight
        auto renderGraph = vsg::createRenderGraphForView(window, camera, universe, VK_SUBPASS_CONTENTS_INLINE, false);
        renderGraph->setClearValues(vsg::sRGB_to_linear(clear_color_->value()));

        auto commandGraph = vsg::CommandGraph::create(window, renderGraph);

        viewer->assignRecordAndSubmitTaskAndPresentation({ commandGraph });

        viewer->compile();

        ctx.viewer = viewer;
    }

    void frame(WindowContext& ctx) override {
        // 原版的循环顺序：先 update（动画推进节点矩阵）再 handleEvents
        //（StellarManipulator 读取的是更新后的节点位置）
        ctx.viewer->update();
        ctx.viewer->handleEvents();
        ctx.viewer->recordAndSubmit();
        ctx.viewer->present();
    }

    void onParamChanged(WindowContext& /*ctx*/, const ParamBase& param) override {
        if (&param == numerical_) {
            numerical_test();
        } else if (&param == speed_) {
            for (auto& animation : animations_)
                animation->speed = speed_->value();
        }
    }

private:
    BoolParam* use_coordinate_frame_ = nullptr;
    ChoiceParam* view_matrix_ = nullptr;
    ChoiceParam* distance_preset_ = nullptr;
    ChoiceParam* mode_ = nullptr;
    BoolParam* play_ = nullptr;
    DoubleParam* speed_ = nullptr;
    DoubleParam* ambient_ = nullptr;
    IntParam* num_stars_ = nullptr;
    DoubleParam* near_ = nullptr;
    DoubleParam* far_ = nullptr;
    ColorParam* clear_color_ = nullptr;
    ActionParam* numerical_ = nullptr;

    vsg::Animations animations_;  // 带节点亲和分配器的 vector（Animation.h:109），与裸 std::vector 不同类型
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(CoordinateFrameExample, "vsgcoordinateframe", "nodes",
                        "CoordinateFrame vs MatrixTransform at astronomical scale: two solar systems 1e9 m apart "
                        "(or a worst-case 8.5e25 m), each with orbit/spin animations and viewpoints. Fly between "
                        "them with number keys and watch why a long-double origin beats a giant matrix. Earth is an "
                        "offline Builder sphere (the original's online tile database needs network).");

}  // namespace vsglab::examples
