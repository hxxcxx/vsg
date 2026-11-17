/**
 * @file maths_example.cpp
 * @brief 移植自 vsgExamples/examples/maths/vsgmaths
 *
 * VSG 数学库全览：向量（多视角分量名/下标访问）、矩阵构造（perspective/lookAt/
 * rotate/translate/scale）与乘法顺序、矩阵×向量的作用、平面/球体的相交检测与
 * 平面变换、矩阵求逆的正确性验证、六种坐标朝向约定（X_UP/Y_UP/Z_UP）互转。
 *
 * 与原版无差异（无命令行参数、无窗口，逐行保留）。
 *
 * @author hxxcxx
 * @date 2026-11-17
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
#include <vsg/maths/plane.h>
#include <vsg/maths/transform.h>
#include <vsg/maths/vec2.h>
#include <vsg/maths/vec3.h>
#include <vsg/maths/vec4.h>

#include <cstddef>
#include <iostream>
#include <vector>

namespace vsglab::examples {
namespace {

using framework::ConsoleContext;

template <class M>
bool test_inverse(const M& m) {
    auto im = vsg::inverse(m);
    auto m_mult_im = m * im;
    M identity;
    typename M::value_type delta = {};
    for (std::size_t c = 0; c < m.columns(); ++c) {
        for (std::size_t r = 0; r < m.rows(); ++r) {
            delta += std::abs(m_mult_im[c][r] - identity[c][r]);
        }
    }
    std::cout << "\ntest_inverse()" << std::endl;
    std::cout << "matrix " << m << std::endl;
    std::cout << "inverse " << im << std::endl;
    std::cout << "m_mult_im " << m_mult_im << std::endl;
    std::cout << "delta " << delta << std::endl;
    return true;
}

class MathsExample : public framework::ConsoleExample {
public:
    void run(ConsoleContext& /*ctx*/) override {
        vsg::vec2 v;

        v.x = 10.1f;
        v.y = 12.2f;

        std::cout << "vec2(x=" << v.x << ", y=" << v.y << ")" << std::endl;
        std::cout << "vec2(r=" << v.r << ", g=" << v.g << ")" << std::endl;
        std::cout << "vec2(s=" << v.s << ", t=" << v.t << ")" << std::endl;
        std::cout << "vec2[0]=(" << v[0] << ", " << v[1] << ")" << std::endl;

        vsg::dvec3 n(2.0, 1.0, 0.5);
        std::cout << "n(x=" << n.x << ", y=" << n.y << ", z=" << n.z << ")" << std::endl;

        std::cout << "n = " << n << std::endl;

        vsg::t_vec2<int> iv(2, 1);
        std::cout << "iv = " << iv << std::endl;

        vsg::dvec4 colour(1.0, 0.9, 1.0, 0.5);
        std::cout << "colour = (" << colour << ")" << std::endl;

        vsg::dmat4 mat;
        mat(3, 0) = 102.3;
        std::cout << "mat = " << mat << std::endl;

        vsg::t_mat4<short> cmat;
        std::cout << "cmat = " << cmat << std::endl;
        std::cout << "sizeof(cmat) = " << sizeof(cmat) << std::endl;

        vsg::ref_ptr<vsg::Object> object(new vsg::Object());
        object->setValue("matrix", mat);

        vsg::mat4 new_mat;
        if (object->getValue("matrix", new_mat)) {
            std::cout << "getValue(\"matrix\"" << new_mat << std::endl;
        }

        auto proj = vsg::perspective(vsg::radians(45.0), 2.0, 0.1, 10.0);
        std::cout << "proj = {" << std::endl << proj << "}" << std::endl;

        auto view = vsg::lookAt(vsg::vec3(2.0f, 2.0f, 2.0f), vsg::vec3(0.0f, 0.0f, 0.0f), vsg::vec3(0.0f, 0.0f, 1.0f));
        std::cout << "view = {" << std::endl << view << "}" << std::endl;

        auto rot = vsg::rotate(vsg::radians(45.0f), 0.0f, 0.0f, 1.0f);
        std::cout << "rot = {" << rot << "}" << std::endl;

        constexpr auto trans = vsg::translate(vsg::vec3(1.0f, 2.0f, 3.0f));
        std::cout << "trans = {" << trans << "}" << std::endl;

        constexpr auto scale = vsg::scale(vsg::vec3(1.0f, 2.0f, 3.0f));
        std::cout << "scale = {" << scale << "}" << std::endl;

        // note VSG and OSG matrix multiplication order is reversed.
        auto result = scale * trans * rot;
        std::cout << "result = {" << result << "}" << std::endl;

        vsg::vec2 v2(1.0, 2.0);
        vsg::vec3 v3(1.0, 2.0, 3.0);
        vsg::vec4 v4(1.0, 2.0, 3.0, 1.0);
        std::cout << "v2 = (" << v2 << "), -v2=" << -v2 << ", v2+v2=(" << (v2 + v2) << "), v2-v2=" << (v2 - v2) << ")"
                  << std::endl;
        std::cout << "v3 = (" << v3 << "), -v3=" << -v3 << ", v3+v4=(" << (v3 + v3) << "), v3-v3=(" << (v3 - v3) << ")"
                  << std::endl;
        std::cout << "v4 = (" << v4 << "), -v4=" << -v4 << ", v5+v5=(" << (v4 + v4) << "), v4-v4=" << (v4 - v4) << ")"
                  << std::endl;
        std::cout << "rot * v3 = (" << (rot * v3) << ")" << std::endl;
        std::cout << "rot * v4 = (" << (rot * v4) << ")" << std::endl;
        std::cout << "trans * v3 = (" << (trans * v3) << ")" << std::endl;
        std::cout << "trans * v4 = (" << (trans * v4) << ")" << std::endl;
        std::cout << "scale * v3 = (" << (scale * v3) << ")" << std::endl;
        std::cout << "scale * v4 = (" << (scale * v4) << ")" << std::endl;
        std::cout << "rot * vec3(1.0, 0.0, 0.0) = (" << (rot * vsg::vec3(1.0, 0.0, 0.0)) << ")" << std::endl;
        std::cout << "rot * vec3(0.0, 1.0, 0.0) = (" << (rot * vsg::vec3(0.0, 1.0, 0.0)) << ")" << std::endl;
        std::cout << "rot * vec3(0.0, 0.0, 1.0) = (" << (rot * vsg::vec3(0.0, 0.0, 1.0)) << ")" << std::endl;
        std::cout << std::endl;
        std::cout << "view * vec3(0.0, 0.0, 0.0) = (" << (view * vsg::vec3(0.0, 0.0, 0.0)) << ")" << std::endl;
        std::cout << "view * vec3(2.0, 2.0, 2.0) = (" << (view * vsg::vec3(2.0, 2.0, 2.0)) << ")" << std::endl;
        std::cout << "view * vec3(2.0, 2.0, 3.0) = (" << (view * vsg::vec3(2.0, 2.0, 3.0)) << ")" << std::endl;
        std::cout << "view * vec4(2.0, 2.0, 3.0, 1.0) = (" << (view * vsg::vec4(2.0, 2.0, 3.0, 1.0)) << ")"
                  << std::endl;

        vsg::dmat4 rot_x = vsg::rotate(vsg::radians(45.0), 1.0, 0.0, 0.0);
        vsg::dmat4 rot_y = vsg::rotate(vsg::radians(45.0), 0.0, 1.0, 0.0);
        vsg::dmat4 rot_z = vsg::rotate(vsg::radians(45.0), 0.0, 0.0, 1.0);

        std::cout << "\nrot_x = " << rot_x << std::endl;
        std::cout << "    rot_x * vsg::vec3(0.0, 1.0, 0.0) = " << (rot_x * vsg::dvec3(0.0, 1.0, 0.0)) << std::endl;
        std::cout << "    rot_x * vsg::vec3(0.0, 0.0, 1.0) = " << (rot_x * vsg::dvec3(0.0, 0.0, 1.0)) << std::endl;
        std::cout << "\nrot_y = " << rot_y << std::endl;
        std::cout << "    rot_y * vsg::vec3(1.0, 0.0, 0.0) = " << (rot_y * vsg::dvec3(1.0, 0.0, 0.0)) << std::endl;
        std::cout << "    rot_y * vsg::vec3(0.0, 0.0, 1.0) = " << (rot_y * vsg::dvec3(0.0, 0.0, 1.0)) << std::endl;
        std::cout << "\nrot_z = " << rot_z << std::endl;
        std::cout << "    rot_z * vsg::vec3(1.0, 0.0, 0.0) = " << (rot_z * vsg::dvec3(1.0, 0.0, 0.0)) << std::endl;
        std::cout << "    rot_z * vsg::vec3(0.0, 1.0, 0.0) = " << (rot_z * vsg::dvec3(0.0, 1.0, 0.0)) << std::endl;

        using Polytope = std::vector<vsg::plane>;
        Polytope polytope{
            vsg::plane(1.0, 0.0, 0.0, 1.0),   // left plane
            vsg::plane(-1.0, 0.0, 0.0, 1.0),  // right plane
            vsg::plane(0.0, 1.0, 0.0, 1.0),   // bottom plane
            vsg::plane(0.0, -1.0, 0.0, 1.0)   // top plane
        };

        std::cout << std::endl << "Planes : " << std::endl;
        for (auto& plane : polytope) {
            std::cout << "   plane : " << plane.n << ", " << plane.p << std::endl;
        }

        using Sphres = std::vector<vsg::sphere>;
        Sphres spheres{ vsg::sphere(vsg::vec3(0.0, 0.0, 0.0), 0.5),  vsg::sphere(vsg::vec3(1.0, 0.0, 0.0), 0.5),
                        vsg::sphere(vsg::vec3(-1.0, 0.0, 0.0), 0.5), vsg::sphere(vsg::vec3(-3.0, 0.0, 0.0), 2.0),
                        vsg::sphere(vsg::vec3(2.0, 0.0, 0.0), 0.5),  vsg::sphere(vsg::vec3(2.0, 0.0, 0.0), 1.0),
                        vsg::sphere(vsg::vec3(3.0, 0.0, 0.0), 1.0) };

        for (auto& sphere : spheres) {
            std::cout << "   sphere : " << sphere.center << " " << sphere.radius << " ";
            if (vsg::intersect(polytope, sphere))
                std::cout << "intersects" << std::endl;
            else
                std::cout << "does not intersect" << std::endl;

            for (auto& plane : polytope) {
                std::cout << "      n=" << plane.n << ", p=" << plane.p << ", distance "
                          << vsg::distance(plane, sphere.center) << std::endl;
            }

            std::cout << std::endl;
        }

        //vsg::mat4 plane_trans = vsg::translate(vsg::vec3(1.0, 2.0, 3.0));
        vsg::mat4 plane_trans = vsg::translate(vsg::vec3(1.0, 0.0, 0.0));

        for (auto& plane : polytope) {
            plane.vec = plane.vec * plane_trans;
        }

        std::cout << std::endl << "Transformed Planes : " << std::endl;
        for (auto& plane : polytope) {
            std::cout << "   plane : " << plane.n << ", " << plane.p << std::endl;
        }

        for (auto& sphere : spheres) {
            std::cout << "   sphere : " << sphere.center << " " << sphere.radius << " ";
            if (vsg::intersect(polytope, sphere))
                std::cout << "intersects" << std::endl;
            else
                std::cout << "does not intersect" << std::endl;

            for (auto& plane : polytope) {
                std::cout << "      n=" << plane.n << ", p=" << plane.p << ", distance "
                          << vsg::distance(plane, sphere.center) << std::endl;
            }

            std::cout << std::endl;
        }

        vsg::vec3 pv(1.0, 2.0, 3.0);
        std::cout << "pv * plane_trans = " << (pv * plane_trans) << std::endl;
        std::cout << "plane_trans * pv = " << (plane_trans * pv) << std::endl;

        test_inverse(view);
        test_inverse(rot);
        test_inverse(scale);
        test_inverse(rot_y);
        test_inverse(rot_z);
        test_inverse(plane_trans);

        std::cout << std::endl
                  << "Test vsg::transform(vsg::CoordinateConvention src, vsg::CoordinateConvention dest, vsg::dmat&)"
                  << std::endl;
        vsg::dmat4 x_to_y;
        vsg::transform(vsg::CoordinateConvention::X_UP, vsg::CoordinateConvention::Y_UP, x_to_y);

        vsg::dmat4 x_to_z;
        vsg::transform(vsg::CoordinateConvention::X_UP, vsg::CoordinateConvention::Z_UP, x_to_z);

        vsg::dmat4 y_to_z;
        vsg::transform(vsg::CoordinateConvention::Y_UP, vsg::CoordinateConvention::Z_UP, y_to_z);

        vsg::dmat4 y_to_x;
        vsg::transform(vsg::CoordinateConvention::Y_UP, vsg::CoordinateConvention::X_UP, y_to_x);

        vsg::dmat4 z_to_x;
        vsg::transform(vsg::CoordinateConvention::Z_UP, vsg::CoordinateConvention::X_UP, z_to_x);

        vsg::dmat4 z_to_y;
        vsg::transform(vsg::CoordinateConvention::Z_UP, vsg::CoordinateConvention::Y_UP, z_to_y);

        std::cout << "x_to_y = " << x_to_y << " rotate = " << vsg::rotate(vsg::radians(90.0), 0.0, 0.0, -1.0)
                  << std::endl;
        std::cout << "x_to_z = " << x_to_z << " rotate = " << vsg::rotate(vsg::radians(90.0), 0.0, -1.0, 0.0)
                  << std::endl;
        std::cout << "y_to_x = " << y_to_x << " rotate = " << vsg::rotate(vsg::radians(90.0), 0.0, 0.0, 1.0)
                  << std::endl;
        std::cout << "y_to_z = " << y_to_z << " rotate = " << vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0)
                  << std::endl;
        std::cout << "z_to_x = " << z_to_x << " rotate = " << vsg::rotate(vsg::radians(90.0), 0.0, 1.0, 0.0)
                  << std::endl;
        std::cout << "z_to_y = " << z_to_y << " rotate = " << vsg::rotate(vsg::radians(90.0), -1.0, 0.0, 0.0)
                  << std::endl;

        std::cout << "x_to_y * y_to_x = " << x_to_y * y_to_x << std::endl;
        std::cout << "x_to_z * z_to_x = " << x_to_z * z_to_x << std::endl;
        std::cout << "y_to_x * x_to_y = " << y_to_x * x_to_y << std::endl;
        std::cout << "y_to_z * z_to_y = " << y_to_z * z_to_y << std::endl;
        std::cout << "z_to_x * x_to_z = " << z_to_x * x_to_z << std::endl;
        std::cout << "z_to_y * y_to_z = " << z_to_y * y_to_z << std::endl;
    }
};

}  // namespace

VSGLAB_REGISTER_EXAMPLE(MathsExample, "vsgmaths", "maths",
                        "VSG maths tour: vectors with swizzle-style component names, matrix constructors "
                        "(perspective/lookAt/rotate/translate/scale), matrix×vector application, plane/sphere "
                        "intersection with transformed planes, inverse verification, and the six coordinate "
                        "convention conversions (X_UP/Y_UP/Z_UP).");

}  // namespace vsglab::examples
