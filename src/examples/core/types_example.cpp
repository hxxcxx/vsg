/**
 * @file types_example.cpp
 * @brief 移植自 vsgExamples/examples/core/vsgtypes
 *
 * 基础类型：打印 C++/vsg 类型的 typeid 名称，建立类型 → VkFormat 的映射表，
 * 以及 vsg 数学函数（radians / rotate / translate / length / dot）的用法。
 *
 * @author hxxcxx
 * @date 2026-11-08
 */

#include "framework/console_example.h"
#include "framework/example_registry.h"

#include <vsg/core/Auxiliary.h>
#include <vsg/core/Object.h>
#include <vsg/core/Visitor.h>
#include <vsg/core/observer_ptr.h>
#include <vsg/core/ref_ptr.h>
#include <vsg/io/stream.h>
#include <vsg/maths/mat4.h>
#include <vsg/maths/transform.h>
#include <vsg/maths/vec2.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/vec4.h>
#include <vsg/vk/Instance.h>

#include <iostream>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

// 与原版一致：在 vsg 命名空间中补充各分量类型的向量别名
namespace vsg {
// do we want to cover all the possible combinations?
using ucvec2 = t_vec2<unsigned char>;
using cvec2 = t_vec2<char>;

using usvec2 = t_vec2<unsigned short>;
using svec2 = t_vec2<short>;

using ivec2 = t_vec2<int>;
using uivec2 = t_vec2<unsigned int>;

using ucvec3 = t_vec3<unsigned char>;
using cvec3 = t_vec3<char>;

using usvec3 = t_vec3<unsigned short>;
using svec3 = t_vec3<short>;

using ivec3 = t_vec3<int>;
using uivec3 = t_vec3<unsigned int>;

using ucvec4 = t_vec4<unsigned char>;
using cvec4 = t_vec4<char>;

using usvec4 = t_vec4<unsigned short>;
using svec4 = t_vec4<short>;

using ivec4 = t_vec4<int>;
using uivec4 = t_vec4<unsigned int>;
}  // namespace vsg

namespace vsglab::examples {
namespace {

std::unordered_map<std::type_index, VkFormat> VkFormatTypeMap;

template <typename T>
VkFormat VkFormatForType(T) {
    return VkFormatTypeMap[std::type_index(typeid(T))];
}

template <typename T>
VkFormat VkFormatForType() {
    return VkFormatTypeMap[std::type_index(typeid(T))];
}

class TypesExample : public framework::ConsoleExample {
public:
    void run(framework::ConsoleContext& /*ctx*/) override {
        std::cout << "typeid(char) = " << typeid(char).name() << std::endl;
        std::cout << "typeid(unsigned char) = " << typeid(unsigned char).name() << std::endl;
        std::cout << "typeid(short) = " << typeid(short).name() << std::endl;
        std::cout << "typeid(unsigned short) = " << typeid(unsigned short).name() << std::endl;
        std::cout << "typeid(int) = " << typeid(int).name() << std::endl;
        std::cout << "typeid(unsigned int) = " << typeid(unsigned int).name() << std::endl;
        std::cout << "typeid(float) = " << typeid(float).name() << std::endl;
        std::cout << "typeid(double) = " << typeid(double).name() << std::endl;
        std::cout << "typeid(vsg::vec2) = " << typeid(vsg::vec2).name() << std::endl;
        std::cout << "typeid(vsg::dvec2) = " << typeid(vsg::dvec2).name() << std::endl;
        std::cout << "typeid(vsg::Vec3) = " << typeid(vsg::vec3).name() << std::endl;
        std::cout << "typeid(vsg::Vec4) = " << typeid(vsg::vec4).name() << std::endl;

        VkFormatTypeMap[std::type_index(typeid(unsigned char))] = VK_FORMAT_R8_UINT;
        VkFormatTypeMap[std::type_index(typeid(char))] = VK_FORMAT_R8_SINT;
        VkFormatTypeMap[std::type_index(typeid(short))] = VK_FORMAT_R16_SINT;
        VkFormatTypeMap[std::type_index(typeid(unsigned int))] = VK_FORMAT_R32_UINT;
        VkFormatTypeMap[std::type_index(typeid(int))] = VK_FORMAT_R32_SINT;
        VkFormatTypeMap[std::type_index(typeid(unsigned long))] = VK_FORMAT_R64_UINT;
        VkFormatTypeMap[std::type_index(typeid(long))] = VK_FORMAT_R64_SINT;
        VkFormatTypeMap[std::type_index(typeid(float))] = VK_FORMAT_R32_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(double))] = VK_FORMAT_R64_SFLOAT;

        VkFormatTypeMap[std::type_index(typeid(vsg::vec2))] = VK_FORMAT_R32G32_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(vsg::dvec2))] = VK_FORMAT_R64G64_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(vsg::uivec2))] = VK_FORMAT_R32G32_UINT;
        VkFormatTypeMap[std::type_index(typeid(vsg::ivec2))] = VK_FORMAT_R32G32_SINT;

        VkFormatTypeMap[std::type_index(typeid(vsg::vec3))] = VK_FORMAT_R32G32B32_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(vsg::dvec3))] = VK_FORMAT_R64G64B64_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(vsg::uivec3))] = VK_FORMAT_R32G32B32_UINT;
        VkFormatTypeMap[std::type_index(typeid(vsg::ivec3))] = VK_FORMAT_R32G32B32_SINT;

        VkFormatTypeMap[std::type_index(typeid(vsg::vec4))] = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(vsg::dvec4))] = VK_FORMAT_R64G64B64A64_SFLOAT;
        VkFormatTypeMap[std::type_index(typeid(vsg::uivec4))] = VK_FORMAT_R32G32B32A32_UINT;
        VkFormatTypeMap[std::type_index(typeid(vsg::ivec4))] = VK_FORMAT_R32G32B32A32_SINT;
        VkFormatTypeMap[std::type_index(typeid(vsg::ucvec4))] = VK_FORMAT_R8G8B8A8_UINT;

        std::cout << "VkFormat " << VkFormatForType('c') << std::endl;
        std::cout << "VkFormat " << VkFormatForType<unsigned char>() << std::endl;
        std::cout << "VkFormat " << VkFormatForType<float>() << std::endl;
        std::cout << "VkFormat " << VkFormatForType(1) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(1l) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(1.0f) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(1.0) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::vec2(1.0f, 2.4f)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::dvec2(1.0, 2.4)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::vec3(1.0f, 2.4f, 0.3f)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::dvec3(1.0, 2.4, 0.2)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::vec4(1.0f, 2.4f, 0.1f, 0.2f)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::dvec4(1.0, 2.4, 0.1, 0.2)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::uivec4(1, 2, 0, 0)) << std::endl;
        std::cout << "VkFormat " << VkFormatForType(vsg::ucvec4(1, 2, 0, 0)) << std::endl;

        constexpr float x = 180.0f;
        constexpr float rad1 = vsg::radians(90.0f);
        float rad2 = vsg::radians(x);

        std::cout << "rad1=" << rad1 << std::endl;
        std::cout << "rad2=" << rad2 << std::endl;
        std::cout << "(vsg::radians(x))=" << (vsg::radians(x)) << std::endl;

        auto matrix = vsg::rotate(vsg::radians(90.0), 0.0, 0.0, 1.0);
        std::cout << "matrix = " << matrix << std::endl;

        constexpr vsg::dvec3 v = vsg::dvec3(1.0, 0.0, 0.0);
        auto matrix2 = vsg::rotate(vsg::radians(45.0), v);
        constexpr auto matrix3 = vsg::translate(v);

        std::cout << "matix2 = " << matrix2 << std::endl;
        std::cout << "matix3 = " << matrix3 << std::endl;
        std::cout << "v = " << v << std::endl;
        std::cout << "v[0] = " << v[0] << std::endl;
        std::cout << "v.x = " << v.x << std::endl;
        std::cout << "length(v) = " << vsg::length(v) << std::endl;
        std::cout << "cross(v, dvec3(2.0, 0.0, 0.0)) = " << vsg::dot(v, vsg::dvec3(2.0, 0.0, 0.0)) << std::endl;
    }
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(TypesExample, "vsgtypes", "core",
                        "Fundamental types: typeid names of C++/vsg types, a type to VkFormat lookup table, "
                        "and vsg maths functions (radians, rotate, translate, length, dot).");

}  // namespace vsglab::examples
