#include "framework/example_registry.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace vsglab::framework {
namespace {

std::vector<ExampleDescriptor>& registry() {
    static std::vector<ExampleDescriptor> descriptors;
    return descriptors;
}

/// 课程表：vsgExamples 的全部示例。
/// 分类按 README 的顺序排列（README 未列出的 animation、lighting 放在最后）；
/// 分类内的顺序按学习路径排（core 即为此前定的学习顺序），与原版 CMakeLists 的字母序无关。
struct Category {
    std::string_view name;
    std::vector<std::string_view> ids;
};

const std::vector<Category>& syllabus() {
    static const std::vector<Category> kSyllabus = {
        { "core",
          { "vsgtypes", "vsgpointer", "vsgvalues", "vsgarrays", "vsgvisitor", "vsgvisitorcustomtype", "vsgmemory",
            "vsgallocator" } },
        { "nodes",
          { "vsggroups", "vsgtransform", "vsglod", "vsglayers", "vsgcoordinateframe", "vsgannotation",
            "vsgtextureprojection", "vsgpagedlod", "vsgtiledatabase" } },
        { "maths", { "vsgmaths" } },
        { "app",
          { "vsgheadless",
            "vsgmultigpu",
            "vsgmultiviews",
            "vsgortho",
            "vsgoverlay",
            "vsgrendertotexture",
            "vsgrendertotexturearray",
            "vsgscreenshot",
            "vsgoffscreenshot",
            "vsgsubpass",
            "vsgturntablecamera",
            "vsgskybox",
            "vsgviewer",
            "vsgdeviceselection",
            "vsganaglyphicstereo",
            "vsgwindows",
            "vsgcameras",
            "vsgvalidate",
            "vsgaxes",
            "vsgcolorspace",
            "vsgmultiwindowmultiview",
            "vsgrenderoutline",
            "vsghelloworld",
            "vsgdepthpeeling" } },
        { "vk", { "vsgcompute" } },
        { "text", { "vsgtext", "vsgtextgroup" } },
        { "raytracing", { "vsgraytracing" } },
        { "state",
          { "vsgdynamictexture", "vsgdynamictexture_cs", "vsgtexturearray", "vsgstateswitch", "vsgcomputevertex",
            "vsgdynamicvertex", "vsgclip", "vsgextendstate", "vsgsampler", "vsgwireframe", "vsgdynamicstate" } },
        { "commands",
          { "vsgdraw", "vsginterleaved", "vsgexecutecommands", "vsgtimestamps", "vsgocclusionquery",
            "vsgdrawindirect" } },
        { "io", { "vsgcluster", "vsgio", "vsglog", "vsglog_mt", "vsgpath", "vsgcustombuilders" } },
        { "ui", { "vsginput", "vsgimgui_example" } },
        { "platform", { "vsgandroidnative", "vsgwin32" } },
        { "utils",
          { "vsgbuilder", "vsggraphicspipelineconfigurator", "vsgshaderset", "vsgintersection", "vsgstoragebuffer",
            "vsgcustomshaderset", "vsginstrumentation", "vsghighlight", "vsgtracyinstrumentation" } },
        { "threading", { "vsgdynamicload", "vsgdynamicviews", "vsgdynamicwindows" } },
        { "volume", { "vsgvolume" } },
        { "meshshaders", { "vsgmeshshader", "vsgmeshshaderset" } },
        { "animation", { "vsganimation" } },
        { "lighting", { "vsglights", "vsgshadow" } },
    };
    return kSyllabus;
}

/// 排序键：(分类位置, 分类内位置)。课程表外的分类排在最后，课程表外的示例排在所在分类的最后
std::pair<size_t, size_t> syllabusPosition(const ExampleInfo& info) {
    const auto& categories = syllabus();
    for (size_t c = 0; c < categories.size(); ++c) {
        if (categories[c].name != info.category)
            continue;
        const auto& ids = categories[c].ids;
        return { c, static_cast<size_t>(std::find(ids.begin(), ids.end(), info.id) - ids.begin()) };
    }
    return { categories.size(), 0 };
}

}  // namespace

bool registerExample(ExampleDescriptor descriptor) {
    registry().push_back(std::move(descriptor));
    return true;
}

const std::vector<ExampleDescriptor>& exampleDescriptors() {
    // 首次调用发生在 main() 之后，此时所有示例都已登记
    static const std::vector<ExampleDescriptor> kSorted = [] {
        auto descriptors = registry();
        std::stable_sort(descriptors.begin(), descriptors.end(),
                         [](const ExampleDescriptor& a, const ExampleDescriptor& b) {
                             const auto pa = syllabusPosition(a.info);
                             const auto pb = syllabusPosition(b.info);
                             if (pa != pb)
                                 return pa < pb;
                             // 课程表外的分类/示例之间按名称排序
                             if (a.info.category != b.info.category)
                                 return a.info.category < b.info.category;
                             return a.info.id < b.info.id;
                         });
        return descriptors;
    }();
    return kSorted;
}

}  // namespace vsglab::framework
