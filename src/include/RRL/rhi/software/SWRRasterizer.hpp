// RRL/include/rhi/software/SWRRasterizer.hpp
#pragma once

#include "RRL/asset/ImageAsset.hpp"
#include "RRL/asset/MeshAsset.hpp"
#include "RRL/rhi/software/SWRTypes.hpp"

namespace rrl::rhi::software {

/**
 * @brief This caches the layout properties of a rrl::asset::ImageAsset target to 
 *  correctly map the rendered colors into the final image.
 */
struct ColorFormatCache {
    int channels;
    uint8_t r_off;
    uint8_t g_off;
    uint8_t b_off;
};


// --- Pixel Helpers -----------------------------------------------
inline size_t GetPixelOffset(int x, int y, int width, int channels) {
    return (y * width + x) * channels;
}
inline float& GetDepth(rrl::asset::ImageAsset& depth_buffer, int x, int y) {
    float* depth_data = reinterpret_cast<float*>(depth_buffer.data.data());
    return depth_data[y * depth_buffer.width + x];
}
inline void SetPixel(rrl::asset::ImageAsset& render_target, int x, int y, uint8_t r, uint8_t g, uint8_t b, const ColorFormatCache& format) {
    size_t offset = GetPixelOffset(x, y, render_target.width, format.channels);
    render_target.data[offset + format.r_off] = r;
    render_target.data[offset + format.g_off] = g;
    render_target.data[offset + format.b_off] = b;
    if (format.channels == 4) render_target.data[offset + 3] = 255;
}



// --- API ---------------------------------------------------------

/**
 * @brief Helper to actually compute the ColorFormatCache from an Image layout.
 */
ColorFormatCache GetColorFormatCache(rrl::asset::ImageColorLayout layout, rrl::asset::ImageChannelLayout channels);

/**
 * @brief Funtion to load standar SoA MeshAsset into tiled AoSoA SIMD friendly packed data structure.
 */
void LoadMeshAssetIntoSWRMesh(const rrl::asset::MeshAsset& mesh_data, SWRMesh& mesh_swr);


// Primitive drawing

/**
 * @brief Rasterizes a 3D point as a fixed-radius 2D circle with depth testing.
 */
void SWRDrawPoint(
    rrl::asset::ImageAsset& render_target, rrl::asset::ImageAsset& depth_buffer, 
    const glm::vec3& p, int radius, const glm::vec3& color, const ColorFormatCache& format
);

/**
 * @brief Rasterizes a 3D line using Bresenham's algorithm with depth testing.
 */
void SWRDrawLine(
    rrl::asset::ImageAsset& render_target, rrl::asset::ImageAsset& depth_buffer, 
    const glm::vec3& p0, const glm::vec3& p1, 
    const glm::vec3& color, const ColorFormatCache& format
);

/**
 * @brief Rasterizes an unfilled triangle using Bresenham lines.
 */
void SWRDrawWireframeTriangle(
    rrl::asset::ImageAsset& render_target, rrl::asset::ImageAsset& depth_buffer, 
    const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
    const glm::vec3& color, const ColorFormatCache& format
);

/**
 * @brief Resizes and blits a 2D texture onto the target framebuffer.
 */
void SWRDrawTexture2D(
    rrl::asset::ImageAsset& render_target, const rrl::asset::ImageAsset& source_tex,
    int px, int py, int pw, int ph,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format
);


// 3D Rendering

/**
 * @brief Software rendering vertex shader. It handles the NDC vertex projection.
 */
void SWRVertexShader(const SWRMesh& mesh, const glm::mat4& mvp, SWRVertexBuffer& out_buffer);

/**
 * @brief Software rendering triangle rasterization.
 */
void RasterizeTriangle(
    rrl::asset::ImageAsset& render_target, rrl::asset::ImageAsset& depth_buffer, 
    const glm::vec4& v0, const glm::vec4& v1, const glm::vec4& v2, 
    const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, 
    const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2, 
    const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2, 
    float iw0, float iw1, float iw2,
    const glm::vec4& mat_base_color, const rrl::asset::ImageAsset* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override 
);

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
    rrl::asset::ImageAsset& render_target, rrl::asset::ImageAsset& depth_buffer, 
    const SWRMesh& mesh, const SWRVertexBuffer& vertex_buffer,
    uint32_t index_offset, uint32_t index_count,
    const glm::vec4& mat_base_color, const rrl::asset::ImageAsset* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override, bool draw_wireframes
);


} // namespace rrl::rhi::software