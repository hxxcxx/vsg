#pragma once

/**
 * @file tile_reader.h
 * @brief 自定义 ReaderWriter：手工搭建瓦片金字塔与 PagedLOD（TileDatabase 的"打开引擎盖"版）
 *
 * 移植自 vsgExamples/examples/nodes/vsgpagedlod/TileReader.h。
 * 通过 options->add(tileReader) 注册后，读取 "root.tile" 即从根瓦片开始构建：
 * read_root/read_subtile 按 {z}/{x}/{y} 或 {quadkey} 模板取图、createECEFTile
 * 生成贴到椭球上的几何、PagedLOD 挂接子瓦片实现分页。带读取计时统计。
 * 原版为全局类 TileReader，此处移入 vsglab::examples::pagedlod；
 * 新增 {quadkey} 模板支持（Bing 直连端点用四叉键编号）。
 *
 * @author hxxcxx
 * @date 2026-11-16
 */

#include <vsg/all.h>

namespace vsglab::examples::pagedlod {
class TileReader : public vsg::Inherit<vsg::ReaderWriter, TileReader> {
public:
    // defaults for readymap
    vsg::dbox extents = { { -180.0, -90.0, 0.0 }, { 180.0, 90.0, 1.0 } };
    uint32_t noX = 2;
    uint32_t noY = 1;
    uint32_t maxLevel = 22;
    bool originTopLeft = true;
    double lodTransitionScreenHeightRatio = 0.25;

    std::string projection;
    vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel = vsg::EllipsoidModel::create();

    vsg::Path imageLayer;
    vsg::Path terrainLayer;
    uint32_t mipmapLevelsHint = 16;

    void init();

    vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename,
                                   vsg::ref_ptr<const vsg::Options> options = {}) const override;

    // timing stats
    mutable std::mutex statsMutex;
    mutable uint64_t numTilesRead{ 0 };
    mutable double totalTimeReadingTiles{ 0.0 };

protected:
    vsg::dvec3 computeLatitudeLongitudeAltitude(const vsg::dvec3& src) const;
    vsg::dbox computeTileExtents(uint32_t x, uint32_t y, uint32_t level) const;
    vsg::Path getTilePath(const vsg::Path& src, uint32_t x, uint32_t y, uint32_t level) const;

    vsg::ref_ptr<vsg::Object> read_root(vsg::ref_ptr<const vsg::Options> options = {}) const;
    vsg::ref_ptr<vsg::Object> read_subtile(uint32_t x, uint32_t y, uint32_t lod,
                                           vsg::ref_ptr<const vsg::Options> options = {}) const;

    vsg::ref_ptr<vsg::Node> createTile(const vsg::dbox& tile_extents, vsg::ref_ptr<vsg::Data> sourceData) const;
    vsg::ref_ptr<vsg::Node> createECEFTile(const vsg::dbox& tile_extents, vsg::ref_ptr<vsg::Data> sourceData) const;
    vsg::ref_ptr<vsg::Node> createTextureQuad(const vsg::dbox& tile_extents, vsg::ref_ptr<vsg::Data> sourceData) const;

    vsg::ref_ptr<vsg::StateGroup> createRoot() const;

    vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout;
    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout;
    vsg::ref_ptr<vsg::Sampler> sampler;
};
}  // namespace vsglab::examples::pagedlod
