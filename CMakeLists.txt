cmake_minimum_required(VERSION 3.24)

project(facet VERSION 0.1.0 LANGUAGES CXX)

# ---- 本机依赖路径（可被 cmake -D 或 preset 覆盖）----
set(FACET_OCCT_ROOT     "F:/dev/occ8.0.1/opencascade"      CACHE PATH "OCCT 8.0.1 根目录（含 inc/ win64/）")
set(FACET_OCCT_3RDPARTY "F:/dev/occ8.0.1/3rdparty-vc14-64" CACHE PATH "OCCT 第三方依赖根")
set(FACET_QT_ROOT       "F:/dev/qt6/6.8.3/msvc2022_64"     CACHE PATH "Qt6 根目录")
set(vsg_DIR             "F:/code2028/VSG/VulkanSceneGraph/install/lib/cmake/vsg" CACHE PATH "VSG CMake 包目录")

list(PREPEND CMAKE_PREFIX_PATH "${FACET_QT_ROOT}")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCASCADE REQUIRED NO_DEFAULT_PATH PATHS "${FACET_OCCT_ROOT}/cmake"
             COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)
find_package(Vulkan REQUIRED)   # vsg 静态库的链接依赖
find_package(vsg REQUIRED)

# MSVC: 统一 UTF-8 源码（Qt6/OCCT 头均为 UTF-8）
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(CMAKE_AUTOMOC ON)

add_executable(facet WIN32
    src/app/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/render/SceneBuilder.h
    src/render/SceneBuilder.cpp
    src/render/VsgViewport.h
    src/render/VsgViewport.cpp
    src/kernel/Kernel.h
    src/kernel/Kernel.cpp
    src/core/MeshData.h
)

target_compile_features(facet PRIVATE cxx_std_17)
target_include_directories(facet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(facet PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    vsg::vsg
    # OCCT（导入目标自带传递依赖与 include 路径）
    TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep
    TKGeomAlgo TKTopAlgo TKPrim TKBool TKFillet TKMesh TKShHealing
)

# ---- 运行时依赖部署 ----
if (WIN32)
    file(GLOB FACET_OCCT_DLLS "${FACET_OCCT_ROOT}/win64/vc14/bin/TK*.dll")
    # OCCT 的 vc14 二进制还依赖 3rdparty 里的 tbb12/jemalloc（TKMesh/TKBool 并行用到）
    file(GLOB FACET_OCCT_TBB_DLL "${FACET_OCCT_3RDPARTY}/tbb-*-x64/bin/tbb12.dll")
    file(GLOB FACET_OCCT_JEMALLOC_DLL "${FACET_OCCT_3RDPARTY}/jemalloc-vc14-64/bin/jemalloc.dll")
    add_custom_command(TARGET facet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FACET_OCCT_DLLS} ${FACET_OCCT_TBB_DLL} ${FACET_OCCT_JEMALLOC_DLL}
                "$<TARGET_FILE_DIR:facet>"
        COMMAND "${FACET_QT_ROOT}/bin/windeployqt.exe" --no-translations "$<TARGET_FILE:facet>"
        COMMENT "Deploying OCCT/Qt runtime DLLs"
        VERBATIM
    )
endif()
