cmake_minimum_required(VERSION 3.24)

# VSG 只装了 Debug(vsgd) 与 RelWithDebInfo(vsgrd) 两种库，工程也只提供这两种配置
set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo" CACHE STRING "" FORCE)

project(vsglab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# MSVC: 源文件使用 UTF-8，并允许单个 target 并行编译多个翻译单元。
if(MSVC)
    # 不指定 /MP 的进程数，由 MSVC 根据当前机器和 MSBuild 调度自行决定。
    # /FS 用于协调并行编译进程对共享 PDB 文件的写入。
    add_compile_options(/utf-8 /MP /FS)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 本机依赖路径（可被 cmake -D 或 preset 覆盖）
set(VSGLAB_QT_ROOT "F:/dev/qt6/6.8.3/msvc2022_64" CACHE PATH "Qt6 根目录")
set(vsg_DIR "F:/code2028/VSG/VulkanSceneGraph/install/lib/cmake/vsg" CACHE PATH "VSG CMake 包目录")
# vsgConfig 依赖 glslang。该 glslang 由 MSVC 14.51 编译，最终链接须使用不低于它的工具链（VS 2026）。
set(glslang_DIR "F:/APP/vcpkg/installed/x64-windows/share/glslang" CACHE PATH "glslang CMake 包目录")

list(PREPEND CMAKE_PREFIX_PATH "${VSGLAB_QT_ROOT}")

# glslang/Qt 只有 Debug 与 Release；RelWithDebInfo 优先匹配 vsgrd，其余回落到 Release；
# 末尾空项让 Vulkan::Vulkan 这类只有无配置 IMPORTED_LOCATION 的目标仍可用
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release "")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Vulkan REQUIRED)
find_package(vsg REQUIRED)

set(CMAKE_AUTOMOC ON)

set(VSGLAB_SOURCES
    src/app/main.cpp
    src/app/main_window.cpp
    src/app/main_window.h
    src/app/output_panel.cpp
    src/app/output_panel.h
    src/app/param_panel.cpp
    src/app/param_panel.h

    src/host/key_map.cpp
    src/host/key_map.h
    src/host/log_bridge.cpp
    src/host/log_bridge.h
    src/host/native_surface.cpp
    src/host/native_surface.h
    src/host/vsg_host.cpp
    src/host/vsg_host.h
    src/host/platform/native_window_handle.h

    src/framework/example.h
    src/framework/example_context.h
    src/framework/example_registry.cpp
    src/framework/example_registry.h
    src/framework/param_set.cpp
    src/framework/param_set.h
    src/framework/window_params.cpp
    src/framework/window_params.h

    src/examples/app/hello_world_example.cpp
)

if(WIN32)
    list(APPEND VSGLAB_SOURCES src/host/platform/native_window_handle_win32.cpp)
elseif(APPLE)
    list(APPEND VSGLAB_SOURCES src/host/platform/native_window_handle_macos.cpp)
else()
    list(APPEND VSGLAB_SOURCES src/host/platform/native_window_handle_x11.cpp)
endif()

add_executable(vsglab ${VSGLAB_SOURCES})

target_include_directories(vsglab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(vsglab PRIVATE
    VSGLAB_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vsgExamples/data"
    VSGLAB_EXAMPLES_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vsgExamples/examples"
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

target_link_libraries(vsglab PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    vsg::vsg
)

# 运行时依赖部署
# VC 运行库随 VS 安装在本机，不由 windeployqt 部署（--no-compiler-runtime）；分发到其他机器时需配合 vc_redist。
if(WIN32)
    if(TARGET Qt6::windeployqt)
        set(VSGLAB_WINDEPLOYQT_EXECUTABLE "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        set(VSGLAB_WINDEPLOYQT_EXECUTABLE "${VSGLAB_QT_ROOT}/bin/windeployqt.exe")
    endif()

    add_custom_command(TARGET vsglab POST_BUILD
        COMMAND "${VSGLAB_WINDEPLOYQT_EXECUTABLE}"
            --no-translations --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime
            "$<TARGET_FILE:vsglab>"
        COMMENT "Deploying Qt runtime via windeployqt..."
        VERBATIM
    )
endif()
