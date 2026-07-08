// RRL/include/rhi/software/SWRRasterizer.cpp

#include "RRL/rhi/software/SWRRasterizer.hpp"

#include "RRL/rhi/software/SIMDTypes.hpp"
#include "RRL/rhi/software/SWRMath.hpp"

#include "RRL/data/ImageData.hpp"
#include <algorithm>

#include "RRL/DebugMacros.hpp"


#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>


namespace rrl::rhi::software {


// --- Image Helpers -----------------------------------------------
enum class RGBAOffsets { R = 0, G = 1, B = 2, A = 3 };
static inline int GetNumChannels(rrl::data::ImageChannelLayout channels) {
    switch (channels) {
        case rrl::data::ImageChannelLayout::CH_1:  return 1;
        case rrl::data::ImageChannelLayout::CH_2:  return 2;
        case rrl::data::ImageChannelLayout::CH_3:  return 3;
        case rrl::data::ImageChannelLayout::CH_4:  return 4;
        default: return 0;
    }
}
static inline uint8_t GetRGBOffset(rrl::data::ImageColorLayout layout, RGBAOffsets offset) {
    switch (layout) {
        case rrl::data::ImageColorLayout::RGB:
        case rrl::data::ImageColorLayout::RGBA: 
            switch (offset) {
                case RGBAOffsets::R: return 0;
                case RGBAOffsets::G: return 1;
                case RGBAOffsets::B: return 2;
                case RGBAOffsets::A: return 3;
            }
            break;

        case rrl::data::ImageColorLayout::BGR:
        case rrl::data::ImageColorLayout::BGRA: 
            switch (offset) {
                case RGBAOffsets::R: return 2;
                case RGBAOffsets::G: return 1;
                case RGBAOffsets::B: return 0;
                case RGBAOffsets::A: return 3;
            }
            break;

        case rrl::data::ImageColorLayout::HSV:
            switch (offset) {
                case RGBAOffsets::R: return 2;
                case RGBAOffsets::G: return 1;
                case RGBAOffsets::B: return 0;
                case RGBAOffsets::A: return 0; // Safe fallback
            }
            break;
            
        default: return 0;
    }
    return 0; 
}
static inline stbir_pixel_layout GetSTBPixelLayout(rrl::data::ImageColorLayout layout, int channels) {
    if (channels == 3) {
        return (layout == rrl::data::ImageColorLayout::BGR) ? STBIR_BGR : STBIR_RGB;
    } else if (channels == 4) {
        return (layout == rrl::data::ImageColorLayout::BGRA) ? STBIR_BGRA : STBIR_RGBA;
    }
    return (channels == 1) ? STBIR_1CHANNEL : STBIR_2CHANNEL;
}


// --- SWRMesh Helpers ---------------------------------------------
// Converts from AoSoA block to scalar for the rasterizer
static inline glm::vec2 GetUV(const SWRMesh& mesh, uint32_t index) {
    if (mesh.uvs.empty()) return glm::vec2(0.0f);
    size_t b_idx = index / SWRMesh::BlockSize;
    size_t l_idx = index % SWRMesh::BlockSize;
    
    #ifdef RRL_SWR_FIXED_POINT
    return glm::vec2(
        static_cast<float>(mesh.uvs[b_idx].x[l_idx]) / 65536.0f,
        static_cast<float>(mesh.uvs[b_idx].y[l_idx]) / 65536.0f
    );
    #else

    return glm::vec2(mesh.uvs[b_idx].x[l_idx], mesh.uvs[b_idx].y[l_idx]);
    #endif
}
static inline glm::vec4 GetColor(const SWRMesh& mesh, uint32_t index) {
    if (mesh.colors.empty()) return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    size_t b_idx = index / SWRMesh::BlockSize;
    size_t l_idx = index % SWRMesh::BlockSize;
    
#ifdef RRL_SWR_FIXED_POINT
    constexpr float INV_FIXED_SCALE = 1.0f / 65536.0f;
    return glm::vec4(
        static_cast<float>(mesh.colors[b_idx].x[l_idx]) * INV_FIXED_SCALE,
        static_cast<float>(mesh.colors[b_idx].y[l_idx]) * INV_FIXED_SCALE,
        static_cast<float>(mesh.colors[b_idx].z[l_idx]) * INV_FIXED_SCALE,
        static_cast<float>(mesh.colors[b_idx].w[l_idx]) * INV_FIXED_SCALE
    );
#else
    return glm::vec4(
        mesh.colors[b_idx].x[l_idx],
        mesh.colors[b_idx].y[l_idx],
        mesh.colors[b_idx].z[l_idx],
        mesh.colors[b_idx].w[l_idx]
    );
#endif
}





// --- API ---------------------------------------------------------
ColorFormatCache GetColorFormatCache(rrl::data::ImageColorLayout layout, rrl::data::ImageChannelLayout channels) {
    return {
        GetNumChannels(channels),GetRGBOffset(layout, RGBAOffsets::R),GetRGBOffset(layout, RGBAOffsets::G), GetRGBOffset(layout, RGBAOffsets::B)
    };
}
void LoadMeshDataIntoSWRMesh(const rrl::data::MeshData& mesh_data, SWRMesh& mesh_swr) {
    mesh_swr.topology               = mesh_data.topology;
    mesh_swr.indices                = mesh_data.indices;
    mesh_swr.submeshes              = mesh_data.submeshes;
    mesh_swr.active_vertex_count    = mesh_data.positions.size();
    if (mesh_swr.active_vertex_count == 0) return;

    // Calculate the exact number of SIMD blocks needed
    size_t block_count = (mesh_swr.active_vertex_count + SWRMesh::BlockSize - 1) / SWRMesh::BlockSize;

    // Allocate and zero-initialize the blocks. 
    mesh_swr.positions.assign(block_count, swr_vec3{});
    if (!mesh_data.normals.empty()) mesh_swr.normals.assign(block_count, swr_vec3{});
    if (!mesh_data.uvs.empty())     mesh_swr.uvs.assign(block_count, swr_vec2{});
    if (!mesh_data.colors.empty())  mesh_swr.colors.assign(block_count, swr_vec4{});

    // Unpack and interleave the data into the SIMD lanes
    for (size_t i = 0; i < mesh_swr.active_vertex_count; ++i) {
        size_t b_idx = i / SWRMesh::BlockSize;
        size_t l_idx = i % SWRMesh::BlockSize;

        #ifdef RRL_SWR_FIXED_POINT

        // Fixed-Point loading
        constexpr float RRL_SWR_FIXED_POINT_SCALE = 65536.0f;

        mesh_swr.positions[b_idx].x[l_idx] = static_cast<int32_t>(mesh_data.positions[i].x * RRL_SWR_FIXED_POINT_SCALE);
        mesh_swr.positions[b_idx].y[l_idx] = static_cast<int32_t>(mesh_data.positions[i].y * RRL_SWR_FIXED_POINT_SCALE);
        mesh_swr.positions[b_idx].z[l_idx] = static_cast<int32_t>(mesh_data.positions[i].z * RRL_SWR_FIXED_POINT_SCALE);

        if (!mesh_data.normals.empty()) {
            mesh_swr.normals[b_idx].x[l_idx] = static_cast<int32_t>(mesh_data.normals[i].x * RRL_SWR_FIXED_POINT_SCALE);
            mesh_swr.normals[b_idx].y[l_idx] = static_cast<int32_t>(mesh_data.normals[i].y * RRL_SWR_FIXED_POINT_SCALE);
            mesh_swr.normals[b_idx].z[l_idx] = static_cast<int32_t>(mesh_data.normals[i].z * RRL_SWR_FIXED_POINT_SCALE);
        }
        if (!mesh_data.uvs.empty()) {
            mesh_swr.uvs[b_idx].x[l_idx] = static_cast<int32_t>(mesh_data.uvs[i].x * RRL_SWR_FIXED_POINT_SCALE);
            mesh_swr.uvs[b_idx].y[l_idx] = static_cast<int32_t>(mesh_data.uvs[i].y * RRL_SWR_FIXED_POINT_SCALE);
        }
        if (!mesh_data.colors.empty()) {
            mesh_swr.colors[b_idx].x[l_idx] = static_cast<int32_t>(mesh_data.colors[i].x * RRL_SWR_FIXED_POINT_SCALE);
            mesh_swr.colors[b_idx].y[l_idx] = static_cast<int32_t>(mesh_data.colors[i].y * RRL_SWR_FIXED_POINT_SCALE);
            mesh_swr.colors[b_idx].z[l_idx] = static_cast<int32_t>(mesh_data.colors[i].z * RRL_SWR_FIXED_POINT_SCALE);
            mesh_swr.colors[b_idx].w[l_idx] = static_cast<int32_t>(mesh_data.colors[i].w * RRL_SWR_FIXED_POINT_SCALE);
        }
        #else

        // Floating-Point loading
        mesh_swr.positions[b_idx].x[l_idx] = mesh_data.positions[i].x;
        mesh_swr.positions[b_idx].y[l_idx] = mesh_data.positions[i].y;
        mesh_swr.positions[b_idx].z[l_idx] = mesh_data.positions[i].z;

        if (!mesh_data.normals.empty()) {
            mesh_swr.normals[b_idx].x[l_idx] = mesh_data.normals[i].x;
            mesh_swr.normals[b_idx].y[l_idx] = mesh_data.normals[i].y;
            mesh_swr.normals[b_idx].z[l_idx] = mesh_data.normals[i].z;
        }
        if (!mesh_data.uvs.empty()) {
            mesh_swr.uvs[b_idx].x[l_idx] = mesh_data.uvs[i].x;
            mesh_swr.uvs[b_idx].y[l_idx] = mesh_data.uvs[i].y;
        }
        if (!mesh_data.colors.empty()) {
            mesh_swr.colors[b_idx].x[l_idx] = mesh_data.colors[i].x;
            mesh_swr.colors[b_idx].y[l_idx] = mesh_data.colors[i].y;
            mesh_swr.colors[b_idx].z[l_idx] = mesh_data.colors[i].z;
            mesh_swr.colors[b_idx].w[l_idx] = mesh_data.colors[i].w;
        }
        #endif
    }
}



// Primitive drawing

void SWRDrawPoint(
    rrl::data::ImageData& rt, rrl::data::ImageData& depth, 
    const glm::vec3& p, int radius, const glm::vec3& color, const ColorFormatCache& format) 
{
    int cx = static_cast<int>(p.x);
    int cy = static_cast<int>(p.y);
    float z = p.z;

    int min_x = std::max(0, cx - radius);
    int max_x = std::min(static_cast<int>(rt.width) - 1, cx + radius);
    int min_y = std::max(0, cy - radius);
    int max_y = std::min(static_cast<int>(rt.height) - 1, cy + radius);
    int r2 = radius * radius;
    
    uint8_t r_col = static_cast<uint8_t>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
    uint8_t g_col = static_cast<uint8_t>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
    uint8_t b_col = static_cast<uint8_t>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));

    for (int y = min_y; y <= max_y; ++y) {
        int dy = y - cy;
        int dy2 = dy * dy;
        for (int x = min_x; x <= max_x; ++x) {
            int dx = x - cx;
            if (dx * dx + dy2 <= r2) {
                if (z <= GetDepth(depth, x, y)) {
                    GetDepth(depth, x, y) = z;
                    SetPixel(rt, x, y, r_col, g_col, b_col, format);
                }
            }
        }
    }
}
void SWRDrawLine(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const glm::vec3& p0, const glm::vec3& p1, 
    const glm::vec3& color, const ColorFormatCache& format) 
{
    int x0 = static_cast<int>(p0.x);
    int y0 = static_cast<int>(p0.y);
    float z0 = p0.z;

    int x1 = static_cast<int>(p1.x);
    int y1 = static_cast<int>(p1.y);
    float z1 = p1.z;

    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int dist = std::max(std::abs(dx), std::abs(dy));

    uint8_t r = static_cast<uint8_t>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
    uint8_t g = static_cast<uint8_t>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
    uint8_t b = static_cast<uint8_t>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));
    
    // Single point fallback
    if (dist == 0) {
        if (x0 >= 0 && x0 < render_target.width && y0 >= 0 && y0 < render_target.height) {
            if (z0 <= GetDepth(depth_buffer, x0, y0)) {
                GetDepth(depth_buffer, x0, y0) = z0;
                SetPixel(render_target, x0, y0, r, g, b, format);
            }
        }
        return;
    }

    // Bresenham Interpolation
    for (int i = 0; i <= dist; i++) {
        if (x0 >= 0 && x0 < render_target.width && y0 >= 0 && y0 < render_target.height) {
            float t = static_cast<float>(i) / dist;
            float z = z0 * (1.0f - t) + z1 * t;

            // Z-bias so wireframes cleanly render over solid geometry
            if (z - 0.0005f <= GetDepth(depth_buffer, x0, y0)) {
                GetDepth(depth_buffer, x0, y0) = z; 
                SetPixel(render_target, x0, y0, r, g, b, format);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void SWRDrawWireframeTriangle(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
    const glm::vec3& color, const ColorFormatCache& format) 
{
    SWRDrawLine(render_target, depth_buffer, p0, p1, color, format);
    SWRDrawLine(render_target, depth_buffer, p1, p2, color, format);
    SWRDrawLine(render_target, depth_buffer, p2, p0, color, format);
}
void SWRDrawTexture2D(
    rrl::data::ImageData& rt, const rrl::data::ImageData& tex,
    int px, int py, int pw, int ph,
    const ColorFormatCache& rt_fmt, const ColorFormatCache& tex_fmt)
{
    // Bounds check
    if (tex.data.empty() || pw <= 0 || ph <= 0) return;
    if (px >= static_cast<int>(rt.width) || py >= static_cast<int>(rt.height) || px + pw <= 0 || py + ph <= 0) return;


    // Check if we actually need to resize
    bool needs_resize = (static_cast<uint32_t>(pw) != tex.width || static_cast<uint32_t>(ph) != tex.height);
    std::vector<uint8_t> resized_tex;
    const uint8_t* src_data = tex.data.data(); // Default to original data
    if (needs_resize) {
        resized_tex.resize(pw * ph * tex_fmt.channels);
        stbir_pixel_layout layout = GetSTBPixelLayout(tex.color_layout, tex_fmt.channels);
        stbir_resize_uint8_linear(
            tex.data.data(), tex.width, tex.height, 0,
            resized_tex.data(), pw, ph, 0, 
            layout
        );
        
        src_data = resized_tex.data(); // Point to the newly resized data
    }

    // Manual blit (uses src_data, which points to either the original or resized buffer)
    for (int y = 0; y < ph; ++y) {
        int dst_y = py + y;
        if (dst_y < 0 || dst_y >= static_cast<int>(rt.height)) continue;

        for (int x = 0; x < pw; ++x) {
            int dst_x = px + x;
            if (dst_x < 0 || dst_x >= static_cast<int>(rt.width)) continue;

            size_t src_offset = (y * pw + x) * tex_fmt.channels;
            size_t dst_offset = (dst_y * rt.width + dst_x) * rt_fmt.channels;

            if (tex_fmt.channels == 4 && src_data[src_offset + 3] < 128) continue; 

            // Extract channels using the explicit offsets computed from the source metadata layout
            uint8_t r = src_data[src_offset + tex_fmt.r_off];
            uint8_t g = src_data[src_offset + tex_fmt.g_off];
            uint8_t b = src_data[src_offset + tex_fmt.b_off];

            // Map variables into the explicit offset positions required by the destination layout
            rt.data[dst_offset + rt_fmt.r_off] = r;
            rt.data[dst_offset + rt_fmt.g_off] = g;
            rt.data[dst_offset + rt_fmt.b_off] = b;
            
            if (rt_fmt.channels == 4) {
                rt.data[dst_offset + 3] = 255;
            }
        }
    }
}



// 3D Rendering

void SWRVertexShader(const SWRMesh& mesh, const glm::mat4& mvp, SWRVertexBuffer& out_buffer) {
    out_buffer.Resize(mesh.active_vertex_count);

    // Tmp blocks
    swr_vec3 clip_block;
    swr_vec1 clip_w_block;
    #ifndef RRL_SWR_AFFINE_INTERPOLATION
    swr_vec1 inv_w_block;
    #endif

    // Iterate over the geometry in SIMD chunks
    for (size_t b = 0; b < mesh.positions.size(); ++b) {
        
        // Vertex block projection
        #ifndef RRL_SWR_AFFINE_INTERPOLATION
        math::ProjectVertexBlock(mesh.positions[b], mvp, clip_block, inv_w_block, clip_w_block);
        #else
        math::ProjectVertexBlock(mesh.positions[b], mvp, clip_block, clip_w_block);
        #endif

        // Unpack into the output VertexBuffer
        size_t start_idx = b * SWRMesh::BlockSize;
        for (size_t l = 0; l < SWRMesh::BlockSize; ++l) {
            size_t v_idx = start_idx + l;
            if (v_idx >= mesh.active_vertex_count) break; 

            #ifdef RRL_SWR_FIXED_POINT
            // Fixed-Point Unpacking
            constexpr float INV_FIXED_SCALE = 1.0f / 65536.0f;
            out_buffer.ndc_positions[v_idx] = glm::vec4(
                static_cast<float>(clip_block.x[l]) * INV_FIXED_SCALE, 
                static_cast<float>(clip_block.y[l]) * INV_FIXED_SCALE, 
                static_cast<float>(clip_block.z[l]) * INV_FIXED_SCALE, 
                static_cast<float>(clip_w_block.x[l]) * INV_FIXED_SCALE
            );

            #ifndef RRL_SWR_AFFINE_INTERPOLATION
            out_buffer.inv_w[v_idx] = static_cast<float>(inv_w_block.x[l]) * INV_FIXED_SCALE;
            #endif

            #else

            // Float Unpacking 
            out_buffer.ndc_positions[v_idx] = glm::vec4(
                static_cast<float>(clip_block.x[l]), 
                static_cast<float>(clip_block.y[l]), 
                static_cast<float>(clip_block.z[l]), 
                static_cast<float>(clip_w_block.x[l])
            );

            #ifndef RRL_SWR_AFFINE_INTERPOLATION
            out_buffer.inv_w[v_idx] = static_cast<float>(inv_w_block.x[l]);
            #endif
            #endif
        }
    }
}
void RasterizeTriangle(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const glm::vec4& v0, const glm::vec4& v1, const glm::vec4& v2,  // NDC + ClipW for depth interp
    const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2,  // Screen coords for edge equations
    const SWRMesh& mesh, const SWRVertexBuffer& vertex_buffer, 
    uint32_t i0, uint32_t i1, uint32_t i2,                          // Mesh indices for UV lookups
    const glm::vec4& mat_base_color, const rrl::data::ImageData* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override 
) {
    // Bounding Box Generation
    // Bounding Box Generation (Fixed unsigned/signed mismatches)
    int width_m1  = static_cast<int>(render_target.width) - 1;
    int height_m1 = static_cast<int>(render_target.height) - 1;

    int min_x = std::clamp(static_cast<int>(std::min({p0.x, p1.x, p2.x})), 0, width_m1);
    int max_x = std::clamp(static_cast<int>(std::max({p0.x, p1.x, p2.x})), 0, width_m1);
    int min_y = std::clamp(static_cast<int>(std::min({p0.y, p1.y, p2.y})), 0, height_m1);
    int max_y = std::clamp(static_cast<int>(std::max({p0.y, p1.y, p2.y})), 0, height_m1);

    // Edge Equation Denominator (Triangle Area)
    float denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    if (std::abs(denom) < 0.00001f) return;

    // Resolve Interpolation Weights Branchlessly (Pulling from VertexBuffer!)
    float inv_w0 = 1.0f, inv_w1 = 1.0f, inv_w2 = 1.0f;
    #ifndef RRL_SWR_AFFINE_INTERPOLATION
    if (!runtime_affine_override) {
        inv_w0 = vertex_buffer.inv_w[i0];
        inv_w1 = vertex_buffer.inv_w[i1];
        inv_w2 = vertex_buffer.inv_w[i2];
    }
    #endif

    // Get UVs and Colors
    glm::vec2 uv0(0.0f), uv1(0.0f), uv2(0.0f);
    bool use_textures = !disable_textures && active_albedo && !active_albedo->data.empty() && !mesh.uvs.empty();
    if (use_textures || show_uvs) {
        uv0 = GetUV(mesh, i0);
        uv1 = GetUV(mesh, i1);
        uv2 = GetUV(mesh, i2);
    }
    glm::vec4 c0(1.0f), c1(1.0f), c2(1.0f);
    if (!mesh.colors.empty()) {
        c0 = GetColor(mesh, i0);
        c1 = GetColor(mesh, i1);
        c2 = GetColor(mesh, i2);
    }

    // Pixel Fragment Loop
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float px = static_cast<float>(x) + 0.5f; // Pixel center
            float py = static_cast<float>(y) + 0.5f;

            // Barycentric equations
            float w0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) / denom;
            float w1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) / denom;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue; // The pixel is outside the triangle
            
            // Linear Depth Interpolation for Z-Buffer test
            float interpolated_z = w0 * v0.z + w1 * v1.z + w2 * v2.z;
            if (interpolated_z >= GetDepth(depth_buffer, x, y)) continue; // The pixel is ocluded by other object
            GetDepth(depth_buffer, x, y) = interpolated_z; // Write on the depth buffer


            // Fragment shading logic
            float perspective_w = 1.0f / (w0 * inv_w0 + w1 * inv_w1 + w2 * inv_w2);
            glm::vec4 interp_c = perspective_w * (w0 * c0 * inv_w0 + w1 * c1 * inv_w1 + w2 * c2 * inv_w2);
            glm::vec4 final_color = mat_base_color * interp_c;

            if (show_uvs && !disable_textures && !use_textures) {
                final_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            }

            // Texture Sampling
            if (use_textures) {
                float interp_u = perspective_w * (w0 * uv0.x * inv_w0 + w1 * uv1.x * inv_w1 + w2 * uv2.x * inv_w2);
                float interp_v = perspective_w * (w0 * uv0.y * inv_w0 + w1 * uv1.y * inv_w1 + w2 * uv2.y * inv_w2);
                // interp_v = 1.0f - interp_v; // Flip V

                // Texture Wrapping
                interp_u = interp_u - std::floor(interp_u);
                interp_v = interp_v - std::floor(interp_v);

                int tex_w_m1 = static_cast<int>(active_albedo->width) - 1;
                int tex_h_m1 = static_cast<int>(active_albedo->height) - 1;
                int tx = std::clamp(static_cast<int>(interp_u * active_albedo->width), 0, tex_w_m1);
                int ty = std::clamp(static_cast<int>(interp_v * active_albedo->height), 0, tex_h_m1);

                size_t texel_offset = GetPixelOffset(tx, ty, active_albedo->width, tex_format.channels);
                
                uint8_t tr = active_albedo->data[texel_offset + tex_format.r_off];
                uint8_t tg = active_albedo->data[texel_offset + tex_format.g_off];
                uint8_t tb = active_albedo->data[texel_offset + tex_format.b_off];

                final_color.r *= (tr / 255.0f);
                final_color.g *= (tg / 255.0f);
                final_color.b *= (tb / 255.0f);
            }

            // Output to Framebuffer
            uint8_t out_r = static_cast<uint8_t>(std::clamp(final_color.r * 255.0f, 0.0f, 255.0f));
            uint8_t out_g = static_cast<uint8_t>(std::clamp(final_color.g * 255.0f, 0.0f, 255.0f));
            uint8_t out_b = static_cast<uint8_t>(std::clamp(final_color.b * 255.0f, 0.0f, 255.0f));

            SetPixel(render_target, x, y, out_r, out_g, out_b, rt_format);

        }
    }
}
void SWRRender3DMesh(
    rrl::data::ImageData& render_target, rrl::data::ImageData& depth_buffer, 
    const SWRMesh& mesh, const SWRVertexBuffer& vertex_buffer,
    uint32_t index_offset, uint32_t index_count,
    const glm::vec4& mat_base_color, const rrl::data::ImageData* active_albedo,
    const ColorFormatCache& rt_format, const ColorFormatCache& tex_format,
    bool disable_textures, bool show_uvs, bool runtime_affine_override, bool draw_wireframes
) {
    if (mesh.indices.empty()) return;
    
    RRL_ASSERT(render_target.IsImageModelValid(), "[SWRRender3DMesh] Received a non valid render_target model");
    RRL_ASSERT( (   render_target.color_layout != rrl::data::ImageColorLayout::NONE && 
                    render_target.color_layout != rrl::data::ImageColorLayout::GRAY && 
                    render_target.color_layout != rrl::data::ImageColorLayout::HSV      ), 
        "[SWRRender3DMesh] Received a render_target with non supported color space layout");
    RRL_ASSERT(render_target.data_type == rrl::data::ImageDataType::UINT8, "[SWRRender3DMesh] Received a render_target with data type not equal to UINT8");
    RRL_ASSERT(depth_buffer.IsImageModelValid(), "[SWRRender3DMesh] Received a non valid depth_buffer model");
    RRL_ASSERT(depth_buffer.color_layout == rrl::data::ImageColorLayout::NONE, "[SWRRender3DMesh] Received a depth_buffer with non supported color space layout");
    RRL_ASSERT(depth_buffer.data_type == rrl::data::ImageDataType::FLOAT32, "[SWRRender3DMesh] Received a depth_buffer with data type not equal to FLOAT32");

    // Viewport scaling
    float half_w = static_cast<float>(render_target.width) * 0.5f;
    float half_h = static_cast<float>(render_target.height) * 0.5f;

    size_t step = (mesh.topology == rrl::data::MeshTopology::TRIANGLES) ? 3 : 2;
    uint32_t end_idx = std::min(index_offset + index_count, static_cast<uint32_t>(mesh.indices.size()));
    for (size_t i = index_offset; i < end_idx; i += step) {
        
        // Render triangles (triangle assembling)
        if (step == 3 && i + 2 < mesh.indices.size()) {
            uint32_t i0 = mesh.indices[i];
            uint32_t i1 = mesh.indices[i+1];
            uint32_t i2 = mesh.indices[i+2];

            // Near-Plane Culling
            // .x, .y, .z are NDC coordinates. .w is the original Clip Space W!
            const glm::vec4& v0 = vertex_buffer.ndc_positions[i0];
            const glm::vec4& v1 = vertex_buffer.ndc_positions[i1];
            const glm::vec4& v2 = vertex_buffer.ndc_positions[i2];
            if (v0.w <= 0.001f || v1.w <= 0.001f || v2.w <= 0.001f) continue;

            // Viewport Transform (NDC -> Screen Space) (OpenCV Convention)
            glm::vec2 p0((v0.x + 1.0f) * half_w, (v0.y + 1.0f) * half_h); 
            glm::vec2 p1((v1.x + 1.0f) * half_w, (v1.y + 1.0f) * half_h); 
            glm::vec2 p2((v2.x + 1.0f) * half_w, (v2.y + 1.0f) * half_h); 

            // Backface Culling (do not apply if draw_wireframes)
            if (!draw_wireframes) {
                float area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
                if (area >= -0.00001f) continue; // Counter-Clockwise winding order:
            }

            if (draw_wireframes) {
                glm::vec3 p0_3d(p0.x, p0.y, v0.z);
                glm::vec3 p1_3d(p1.x, p1.y, v1.z);
                glm::vec3 p2_3d(p2.x, p2.y, v2.z);
                
                // Blend base color with vertex colors for the wireframe lines
                glm::vec4 c0 = GetColor(mesh, i0);
                glm::vec3 mat_color = glm::vec3(mat_base_color * c0); 
                SWRDrawWireframeTriangle(render_target, depth_buffer, p0_3d, p1_3d, p2_3d, mat_color, rt_format);

            } 
            else {

                // Get UVs and Colors
                glm::vec2 uv0(0.0f), uv1(0.0f), uv2(0.0f);
                if (!mesh.uvs.empty()) {
                    uv0 = GetUV(mesh, i0); uv1 = GetUV(mesh, i1); uv2 = GetUV(mesh, i2);
                }
                glm::vec4 c0(1.0f), c1(1.0f), c2(1.0f);
                if (!mesh.colors.empty()) {
                    c0 = GetColor(mesh, i0); c1 = GetColor(mesh, i1); c2 = GetColor(mesh, i2);
                }
                
                // Resolve Interpolation Weights 
                float iw0 = 1.0f, iw1 = 1.0f, iw2 = 1.0f;
                #ifndef RRL_SWR_AFFINE_INTERPOLATION
                if (!runtime_affine_override) {
                    iw0 = vertex_buffer.inv_w[i0];
                    iw1 = vertex_buffer.inv_w[i1];
                    iw2 = vertex_buffer.inv_w[i2];
                }
                #endif

                // Dispatch to the Rasterizer
                RasterizeTriangle(
                    render_target, depth_buffer,
                    v0, v1, v2, p0, p1, p2,
                    uv0, uv1, uv2, c0, c1, c2,
                    iw0, iw1, iw2,
                    mat_base_color, active_albedo, 
                    rt_format, tex_format,
                    disable_textures, show_uvs, runtime_affine_override
                );
            }
        }
        
        // Render lines
        else if (step == 2 && i + 1 < mesh.indices.size()) {
            uint32_t i0 = mesh.indices[i];
            uint32_t i1 = mesh.indices[i+1];

            const glm::vec4& v0 = vertex_buffer.ndc_positions[i0];
            const glm::vec4& v1 = vertex_buffer.ndc_positions[i1];
            
            if (v0.w <= 0.001f || v1.w <= 0.001f) continue;

            // Viewport Transform (NDC -> Screen Space) (OpenCV Convention)
            glm::vec3 p0((v0.x + 1.0f) * half_w, (v0.y + 1.0f) * half_h, v0.z);
            glm::vec3 p1((v1.x + 1.0f) * half_w, (v1.y + 1.0f) * half_h, v1.z);

            glm::vec4 c0 = GetColor(mesh, i0);
            glm::vec3 mat_color = glm::vec3(mat_base_color * c0); 

            SWRDrawLine(render_target, depth_buffer, p0, p1, mat_color, rt_format);
        }
    }

}
    

} // namespace rrl::rhi::software