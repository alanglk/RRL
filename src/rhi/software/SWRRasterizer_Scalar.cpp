// RRL/include/rhi/software/SWRRasterizer_Scalar.cpp

#include "RRL/rhi/software/SWRRasterizer.hpp"
#include <glm/glm.hpp>
#include <algorithm>

namespace rrl::rhi::software {

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
) {
    int width_m1  = static_cast<int>(render_target.width) - 1;
    int height_m1 = static_cast<int>(render_target.height) - 1;

    int min_x = std::clamp(static_cast<int>(std::min({p0.x, p1.x, p2.x})), 0, width_m1);
    int max_x = std::clamp(static_cast<int>(std::max({p0.x, p1.x, p2.x})), 0, width_m1);
    int min_y = std::clamp(static_cast<int>(std::min({p0.y, p1.y, p2.y})), 0, height_m1);
    int max_y = std::clamp(static_cast<int>(std::max({p0.y, p1.y, p2.y})), 0, height_m1);

    float denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    if (std::abs(denom) < 0.00001f) return;

    bool use_textures = !disable_textures && active_albedo && !active_albedo->data.empty();

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float py = static_cast<float>(y) + 0.5f;

            float w0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) / denom;
            float w1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) / denom;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue; 
            
            float z = w0 * v0.z + w1 * v1.z + w2 * v2.z;
            if (z >= GetDepth(depth_buffer, x, y)) continue; 
            GetDepth(depth_buffer, x, y) = z; 

            float persp_w = 1.0f / (w0 * iw0 + w1 * iw1 + w2 * iw2);
            glm::vec4 interp_c = persp_w * (w0 * c0 * iw0 + w1 * c1 * iw1 + w2 * c2 * iw2);
            glm::vec4 final_col = mat_base_color * interp_c;

            if (show_uvs && !use_textures) final_col = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

            if (use_textures) {
                float u = persp_w * (w0 * uv0.x * iw0 + w1 * uv1.x * iw1 + w2 * uv2.x * iw2);
                float v = persp_w * (w0 * uv0.y * iw0 + w1 * uv1.y * iw1 + w2 * uv2.y * iw2);

                u = u - std::floor(u);
                v = v - std::floor(v);

                int tx = std::clamp(static_cast<int>(u * active_albedo->width), 0, static_cast<int>(active_albedo->width) - 1);
                int ty = std::clamp(static_cast<int>(v * active_albedo->height), 0, static_cast<int>(active_albedo->height) - 1);

                size_t texel_offset = GetPixelOffset(tx, ty, active_albedo->width, tex_format.channels);
                
                final_col.r *= (active_albedo->data[texel_offset + tex_format.r_off] / 255.0f);
                final_col.g *= (active_albedo->data[texel_offset + tex_format.g_off] / 255.0f);
                final_col.b *= (active_albedo->data[texel_offset + tex_format.b_off] / 255.0f);
            }

            uint8_t r = static_cast<uint8_t>(std::clamp(final_col.r * 255.0f, 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(std::clamp(final_col.g * 255.0f, 0.0f, 255.0f));
            uint8_t b = static_cast<uint8_t>(std::clamp(final_col.b * 255.0f, 0.0f, 255.0f));

            SetPixel(render_target, x, y, r, g, b, rt_format);
        }
    }
}

} // namespace rrl::rhi::software