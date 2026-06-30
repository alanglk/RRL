// RRL/include/rhi/software/SWRTypes.hpp
#pragma once

#include "RRL/data/ImageData.hpp"
#include "RRL/data/MeshData.hpp"
#include "RRL/rhi/software/SWRTypes.hpp"

namespace rrl::rhi::software {

/**
 * @brief This caches the layout properties of a rrl::data::ImageData target to 
 *  correctly map the rendered colors into the final image.
 */
struct ColorFormatCache {
    int channels;
    uint8_t r_off;
    uint8_t g_off;
    uint8_t b_off;
};



// --- API ---------------------------------------------------------

/**
 * @brief Helper to actually compute the ColorFormatCache from an Image layout.
 */
ColorFormatCache GetColorFormatCache(rrl::data::ImageColorLayout layout, rrl::data::ImageChannelLayout channels);

/**
 * @brief Funtion to load standar SoA MeshData into tiled AoSoA SIMD friendly packed data structure.
 */
void LoadMeshDataIntoSWRMesh(const rrl::data::MeshData& mesh_data, SWRMesh& mesh_swr);

/**
 * @brief Software rendering vertex shader. It handles the NDC vertex projection.
 */
void SWRVertexShader(const SWRMesh& mesh, const glm::mat4& mvp, SWRVertexBuffer& out_buffer);

/**
 * @brief Software rendering triangle assembly and rasterization (primitive assembly + fragment shader)
 * It assumes Counter-Clockwise winding order for backface culling.
 * Debug options:
 *  - disable_textures:         Do not render textures
 *  - show_uvs:                 Render uv map coloring
 *  - runtime_affine_override:  Enable affine interpolation instead of default baycentric interpolation
 *  - draw_wireframes:          Just draw the mesh wireframes
 */
void SWRRender3DMesh(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const SWRMesh& mesh, const SWRVertexBuffer& vertex_buffer,
    uint32_t index_offset, uint32_t index_count,
    const glm::vec4& mat_base_color, const rrl::data::ImageData* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override, bool draw_wireframes
);


} // namespace rrl::rhi::software