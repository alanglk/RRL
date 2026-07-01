// RRL/include/rhi/software/SWRRasterizer_Highway.cpp

#ifndef RRL_BUILD_SWR_SIMD_BACKEND 
    #error "This file (SWRRasterizer_Highway.cpp) requires Google Highway support."
#endif

#include "RRL/rhi/software/SWRRasterizer.hpp"
#include <hwy/highway.h>
#include <glm/glm.hpp>
#include <algorithm>

namespace rrl::rhi::software {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

void RasterizeTriangleImpl(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const glm::vec4& v0, const glm::vec4& v1, const glm::vec4& v2, 
    const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, 
    const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2, 
    const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2, 
    float iw0, float iw1, float iw2,
    const glm::vec4& mat_base_color, const rrl::data::ImageData* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override
) {
    int min_x = std::clamp(static_cast<int>(std::min({p0.x, p1.x, p2.x})), 0, static_cast<int>(render_target.width) - 1);
    int max_x = std::clamp(static_cast<int>(std::max({p0.x, p1.x, p2.x})), 0, static_cast<int>(render_target.width) - 1);
    int min_y = std::clamp(static_cast<int>(std::min({p0.y, p1.y, p2.y})), 0, static_cast<int>(render_target.height) - 1);
    int max_y = std::clamp(static_cast<int>(std::max({p0.y, p1.y, p2.y})), 0, static_cast<int>(render_target.height) - 1);

    float denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    if (std::abs(denom) < 0.00001f) return;
    float inv_denom = 1.0f / denom;
    bool use_textures = !disable_textures && active_albedo && !active_albedo->data.empty();

    hn::ScalableTag<float> df;
    hn::ScalableTag<int32_t> di;
    size_t N = hn::Lanes(df);

    auto d_p0x = hn::Set(df, p0.x); auto d_p0y = hn::Set(df, p0.y);
    auto d_p1x = hn::Set(df, p1.x); auto d_p1y = hn::Set(df, p1.y);
    auto d_p2x = hn::Set(df, p2.x); auto d_p2y = hn::Set(df, p2.y);
    auto d_inv_denom = hn::Set(df, inv_denom);

    auto d_v0z = hn::Set(df, v0.z); auto d_v1z = hn::Set(df, v1.z); auto d_v2z = hn::Set(df, v2.z);
    
    auto d_iw0 = hn::Set(df, iw0); auto d_iw1 = hn::Set(df, iw1); auto d_iw2 = hn::Set(df, iw2);
    
    auto d_u0_iw = hn::Set(df, uv0.x * iw0); auto d_u1_iw = hn::Set(df, uv1.x * iw1); auto d_u2_iw = hn::Set(df, uv2.x * iw2);
    auto d_v0_iw = hn::Set(df, uv0.y * iw0); auto d_v1_iw = hn::Set(df, uv1.y * iw1); auto d_v2_iw = hn::Set(df, uv2.y * iw2);
    
    auto d_c0r_iw = hn::Set(df, c0.r * iw0); auto d_c1r_iw = hn::Set(df, c1.r * iw1); auto d_c2r_iw = hn::Set(df, c2.r * iw2);
    auto d_c0g_iw = hn::Set(df, c0.g * iw0); auto d_c1g_iw = hn::Set(df, c1.g * iw1); auto d_c2g_iw = hn::Set(df, c2.g * iw2);
    auto d_c0b_iw = hn::Set(df, c0.b * iw0); auto d_c1b_iw = hn::Set(df, c1.b * iw1); auto d_c2b_iw = hn::Set(df, c2.b * iw2);
    
    auto d_mat_r = hn::Set(df, mat_base_color.r); auto d_mat_g = hn::Set(df, mat_base_color.g); auto d_mat_b = hn::Set(df, mat_base_color.b);

    HWY_ALIGN int32_t ext_mask[256];
    HWY_ALIGN float ext_tx[256]; HWY_ALIGN float ext_ty[256];
    HWY_ALIGN float ext_r[256]; HWY_ALIGN float ext_g[256]; HWY_ALIGN float ext_b[256];

    for (int y = min_y; y <= max_y; ++y) {
        auto py = hn::Set(df, static_cast<float>(y) + 0.5f);

        for (int x = min_x; x <= max_x; x += N) {
            size_t lanes_to_process = std::min(static_cast<size_t>(max_x - x + 1), N);
            auto valid_lanes = hn::FirstN(df, lanes_to_process);

            auto px = hn::Add(hn::Set(df, static_cast<float>(x) + 0.5f), hn::ConvertTo(df, hn::Iota(di, 0)));

            auto w0 = hn::Mul(hn::Add(hn::Mul(hn::Sub(d_p1y, d_p2y), hn::Sub(px, d_p2x)), hn::Mul(hn::Sub(d_p2x, d_p1x), hn::Sub(py, d_p2y))), d_inv_denom);
            auto w1 = hn::Mul(hn::Add(hn::Mul(hn::Sub(d_p2y, d_p0y), hn::Sub(px, d_p2x)), hn::Mul(hn::Sub(d_p0x, d_p2x), hn::Sub(py, d_p2y))), d_inv_denom);
            auto w2 = hn::Sub(hn::Sub(hn::Set(df, 1.0f), w0), w1);

            auto inside = hn::And(valid_lanes, hn::And(hn::Ge(w0, hn::Zero(df)), hn::And(hn::Ge(w1, hn::Zero(df)), hn::Ge(w2, hn::Zero(df)))));
            if (hn::AllFalse(df, inside)) continue;

            // Z buffering and culling
            auto z = hn::Add(hn::Add(hn::Mul(w0, d_v0z), hn::Mul(w1, d_v1z)), hn::Mul(w2, d_v2z));
            float* depth_ptr = &GetDepth(depth_buffer, x, y);
            auto depth_val = hn::MaskedLoad(inside, df, depth_ptr);
            auto z_pass = hn::And(inside, hn::Lt(z, depth_val));
            if (hn::AllFalse(df, z_pass)) continue;
            
            hn::BlendedStore(z, z_pass, df, depth_ptr);

            auto persp_w = hn::Div(hn::Set(df, 1.0f), hn::Add(hn::Add(hn::Mul(w0, d_iw0), hn::Mul(w1, d_iw1)), hn::Mul(w2, d_iw2)));
            
            auto base_r = hn::Mul(hn::Mul(hn::Add(hn::Add(hn::Mul(w0, d_c0r_iw), hn::Mul(w1, d_c1r_iw)), hn::Mul(w2, d_c2r_iw)), persp_w), d_mat_r);
            auto base_g = hn::Mul(hn::Mul(hn::Add(hn::Add(hn::Mul(w0, d_c0g_iw), hn::Mul(w1, d_c1g_iw)), hn::Mul(w2, d_c2g_iw)), persp_w), d_mat_g);
            auto base_b = hn::Mul(hn::Mul(hn::Add(hn::Add(hn::Mul(w0, d_c0b_iw), hn::Mul(w1, d_c1b_iw)), hn::Mul(w2, d_c2b_iw)), persp_w), d_mat_b);

            if (use_textures) {
                auto u = hn::Mul(hn::Add(hn::Add(hn::Mul(w0, d_u0_iw), hn::Mul(w1, d_u1_iw)), hn::Mul(w2, d_u2_iw)), persp_w);
                auto v = hn::Mul(hn::Add(hn::Add(hn::Mul(w0, d_v0_iw), hn::Mul(w1, d_v1_iw)), hn::Mul(w2, d_v2_iw)), persp_w);

                u = hn::Sub(u, hn::Floor(u)); 
                v = hn::Sub(v, hn::Floor(v));
                
                auto tx = hn::Mul(u, hn::Set(df, static_cast<float>(active_albedo->width)));
                auto ty = hn::Mul(v, hn::Set(df, static_cast<float>(active_albedo->height)));
                hn::Store(tx, df, ext_tx);
                hn::Store(ty, df, ext_ty);
            }

            if (show_uvs && !use_textures) {
                base_r = hn::Set(df, 1.0f); base_g = hn::Zero(df); base_b = hn::Zero(df);
            }

            hn::Store(base_r, df, ext_r);
            hn::Store(base_g, df, ext_g);
            hn::Store(base_b, df, ext_b);

            auto active_ints = hn::IfThenElse(hn::RebindMask(di, z_pass), hn::Set(di, 1), hn::Zero(di));
            hn::Store(active_ints, di, ext_mask);

            for (size_t i = 0; i < lanes_to_process; ++i) {
                if (ext_mask[i]) {
                    float final_r = ext_r[i];
                    float final_g = ext_g[i];
                    float final_b = ext_b[i];

                    if (use_textures) {
                        int tx_idx = std::clamp(static_cast<int>(ext_tx[i]), 0, static_cast<int>(active_albedo->width) - 1);
                        int ty_idx = std::clamp(static_cast<int>(ext_ty[i]), 0, static_cast<int>(active_albedo->height) - 1);

                        size_t texel_offset = GetPixelOffset(tx_idx, ty_idx, active_albedo->width, tex_format.channels);
                        
                        final_r *= (active_albedo->data[texel_offset + tex_format.r_off] / 255.0f);
                        final_g *= (active_albedo->data[texel_offset + tex_format.g_off] / 255.0f);
                        final_b *= (active_albedo->data[texel_offset + tex_format.b_off] / 255.0f);
                    }

                    uint8_t out_r = static_cast<uint8_t>(std::clamp(final_r * 255.0f, 0.0f, 255.0f));
                    uint8_t out_g = static_cast<uint8_t>(std::clamp(final_g * 255.0f, 0.0f, 255.0f));
                    uint8_t out_b = static_cast<uint8_t>(std::clamp(final_b * 255.0f, 0.0f, 255.0f));

                    SetPixel(render_target, x + i, y, out_r, out_g, out_b, rt_format);
                }
            }
        }
    }
}
} // namespace HWY_NAMESPACE

// Expose to Header
void RasterizeTriangle(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const glm::vec4& v0, const glm::vec4& v1, const glm::vec4& v2, 
    const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, 
    const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2, 
    const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2, 
    float iw0, float iw1, float iw2,
    const glm::vec4& mat_base_color, const rrl::data::ImageData* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override
) {
    HWY_NAMESPACE::RasterizeTriangleImpl(
        render_target, depth_buffer, v0, v1, v2, p0, p1, p2, 
        uv0, uv1, uv2, c0, c1, c2, iw0, iw1, iw2, 
        mat_base_color, active_albedo, rt_format, tex_format, 
        disable_textures, show_uvs, runtime_affine_override
    );
}

} // namespace rrl::rhi::software