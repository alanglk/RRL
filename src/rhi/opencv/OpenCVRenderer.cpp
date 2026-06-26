// RRL/src/rhi/opencv/OpenCVRenderer.cpp

#ifndef  RRL_RHI_OPENCV
    #error "OpenCVRenderer.hpp file must be compiled with OpenCV support."
#endif

// #define RHI_DEBUG_RENDERING 
#define RHI_TEXTURE_MAPPING

#include "RRL/camera/CameraComponents.hpp"
#include "RRL/tf/TFComponents.hpp"

#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MaterialComponents.hpp"

#include "RRL/rhi/RHILayers.hpp"
#include "RRL/rhi/RHIBackend.hpp"

#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"

#include <glm/glm.hpp>
#include <unordered_map>
#include <limits>


namespace rrl::rhi::opencv {


/**
 * @brief Holds the running OpenCV rendering backend context.
 */
struct OpenCVContext {
    RHIConfig config;
    std::unordered_map<RenderTargetHandle, cv::Mat> render_targets;
    std::unordered_map<RenderTargetHandle, cv::Mat> depth_buffers;

    std::unordered_map<TextureHandle, cv::Mat> textures;
    std::unordered_map<MeshHandle, data::MeshData> meshes;
    std::unordered_map<MaterialHandle, data::MaterialData> materials;

    RenderTargetHandle next_handle  { 1 }; // 0 is reserved for TARGET_MAIN
    TextureHandle next_tex_handle   { 1 };
    MeshHandle next_mesh_handle     { 1 };
    MaterialHandle next_mat_handle  { 1 };
    
    // Debugging flag
    RHIDebugFlag debug_flag { RHIDebugFlag::FLAG_NONE };
};


// --- Software Rendering ------------------------------------------
static void RasterizeLine(
    cv::Mat& render_target, cv::Mat& depth_buffer, 
    const glm::vec3& p0, const glm::vec3& p1, 
    const cv::Scalar& color) 
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
    
    // Point fallback
    if (dist == 0) {
        if (x0 >= 0 && x0 < render_target.cols && y0 >= 0 && y0 < render_target.rows) {
            if (z0 <= depth_buffer.at<float>(y0, x0)) {
                depth_buffer.at<float>(y0, x0) = z0;
                render_target.at<cv::Vec3b>(y0, x0) = cv::Vec3b(color[0], color[1], color[2]);
            }
        }
        return;
    }

    // Standard Bresenham interpolation with depth
    for (int i = 0; i <= dist; i++) {
        if (x0 >= 0 && x0 < render_target.cols && y0 >= 0 && y0 < render_target.rows) {
            float t = static_cast<float>(i) / dist;
            float z = z0 * (1.0f - t) + z1 * t;

            // Add a slight depth bias so wireframes render cleanly over solid triangles
            if (z - 0.0005f <= depth_buffer.at<float>(y0, x0)) {
                depth_buffer.at<float>(y0, x0) = z; // Update Z-Buffer
                render_target.at<cv::Vec3b>(y0, x0) = cv::Vec3b(color[0], color[1], color[2]); // Draw pixel
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void DrawWireframeTriangle(
    cv::Mat& render_target, cv::Mat& depth_buffer, 
    const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
    const cv::Scalar& color) 
{
    RasterizeLine(render_target, depth_buffer, p0, p1, color);
    RasterizeLine(render_target, depth_buffer, p1, p2, color);
    RasterizeLine(render_target, depth_buffer, p2, p0, color);
}
static void RasterizeTriangle(
    cv::Mat& render_target, 
    cv::Mat& depth_buffer, 
    const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2,
    const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2,
    const data::MeshData& mesh, uint32_t i0, uint32_t i1, uint32_t i2,
    const glm::vec4& mat_base_color, const cv::Mat* active_albedo, 
    bool disable_textures, bool show_uvs) 
{
    // Perspective Divide to Normalized Device Coordinates (NDC)
    glm::vec3 ndc0 = glm::vec3(c0) / c0.w;
    glm::vec3 ndc1 = glm::vec3(c1) / c1.w;
    glm::vec3 ndc2 = glm::vec3(c2) / c2.w;

    // Compute Bounding Box over the target matrix screen space
    int min_x = std::clamp(static_cast<int>(std::min({p0.x, p1.x, p2.x})), 0, render_target.cols - 1);
    int max_x = std::clamp(static_cast<int>(std::max({p0.x, p1.x, p2.x})), 0, render_target.cols - 1);
    int min_y = std::clamp(static_cast<int>(std::min({p0.y, p1.y, p2.y})), 0, render_target.rows - 1);
    int max_y = std::clamp(static_cast<int>(std::max({p0.y, p1.y, p2.y})), 0, render_target.rows - 1);

    float denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    if (std::abs(denom) < 0.00001f) return; // Degenerate geometry guard

    // Precompute reciprocal depths for accurate perspective interpolation
    float inv_w0 = 1.0f / c0.w;
    float inv_w1 = 1.0f / c1.w;
    float inv_w2 = 1.0f / c2.w;

    // Loop through every pixel inside the localized bounding box
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float py = static_cast<float>(y) + 0.5f;

            // Calculate screen-space Barycentric Coordinates
            float w0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) / denom;
            float w1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) / denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                // Interpolate linear screen space depth for testing
                float interpolated_z = w0 * ndc0.z + w1 * ndc1.z + w2 * ndc2.z;

                // --- Z-Buffer Pass Check ---
                if (interpolated_z < depth_buffer.at<float>(y, x)) {
                    depth_buffer.at<float>(y, x) = interpolated_z;

                    // Reconstruct the accurate perspective-correct barycentric interpolation factor
                    float perspective_w = 1.0f / (w0 * inv_w0 + w1 * inv_w1 + w2 * inv_w2);

                    // Interpolate vertex attributes using the perspective-corrected weight
                    glm::vec4 vertex_color(1.0f);
                    if (!mesh.colors.empty()) {
                        vertex_color = perspective_w * (w0 * mesh.colors[i0] * inv_w0 + 
                                                       w1 * mesh.colors[i1] * inv_w1 + 
                                                       w2 * mesh.colors[i2] * inv_w2);
                    }

                    glm::vec4 final_color = mat_base_color * vertex_color;
                    
                    // Debug texture mapping / UVs
                    if (show_uvs && !disable_textures) {
                        if (!active_albedo || active_albedo->empty()) {
                            final_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);    // Red
                        } else if (mesh.uvs.empty()) {
                            final_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);    // Blue
                        }
                    }

                    // Execute Texture Atlas Lookup
                    if (!disable_textures && active_albedo && !active_albedo->empty() && !mesh.uvs.empty()) {
                        glm::vec2 uv = perspective_w * (w0 * mesh.uvs[i0] * inv_w0 + 
                                                       w1 * mesh.uvs[i1] * inv_w1 + 
                                                       w2 * mesh.uvs[i2] * inv_w2);

                        uv.x = uv.x - std::floor(uv.x);
                        uv.y = uv.y - std::floor(uv.y);

                        int tx = std::clamp(static_cast<int>(uv.x * active_albedo->cols), 0, active_albedo->cols - 1);
                        int ty = std::clamp(static_cast<int>(uv.y * active_albedo->rows), 0, active_albedo->rows - 1);

                        cv::Vec3b texel = active_albedo->at<cv::Vec3b>(ty, tx);
                        
                        if (texel[0] == 0 && texel[1] == 0 && texel[2] == 0) {
                            final_color = mat_base_color; 
                        } else {
                            final_color.b *= (texel[0] / 255.0f);
                            final_color.g *= (texel[1] / 255.0f);
                            final_color.r *= (texel[2] / 255.0f);
                        }
                    }

                    // Flush finalized colors to the output render target slice
                    render_target.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uint8_t>(std::clamp(final_color.b * 255.0f, 0.0f, 255.0f)),
                        static_cast<uint8_t>(std::clamp(final_color.g * 255.0f, 0.0f, 255.0f)),
                        static_cast<uint8_t>(std::clamp(final_color.r * 255.0f, 0.0f, 255.0f))
                    );
                }
            }
        }
    }
}




// --- Lifecycle ---------------------------------------------------
static bool Initialize(entt::registry& registry, const RHIConfig& config) {
    // Inject the context into the registry
    auto& ctx = registry.ctx().emplace<OpenCVContext>();
    ctx.config = config;

    // Implicitly create TARGET_MAIN
    ctx.render_targets[TARGET_MAIN] = cv::Mat(config.height, config.width, CV_8UC3, cv::Scalar(30, 30, 30));
    ctx.depth_buffers[TARGET_MAIN]  = cv::Mat(config.height, config.width, CV_32FC1, cv::Scalar(std::numeric_limits<float>::max()));

    if (config.mode == RHIRenderingMode::WINDOW) {
        cv::namedWindow(config.title, cv::WINDOW_AUTOSIZE);
    }
    
    LOG_WARN("[OpenCV RHI] CPU-only mode. Expect deep-copy memory overhead during asset synchronization to mimic GPU VRAM uploads.");
    return true;
}
static void Shutdown(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::destroyWindow(ctx.config.title);
    }
    
    // Remove the context entirely, destroying all cv::Mat buffers automatically
    registry.ctx().erase<OpenCVContext>();
}
static void RenderFrame(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();

    // Clear all active targets
    for (auto& [handle, mat] : ctx.render_targets) {
        mat.setTo(cv::Scalar(30, 30, 30));
        ctx.depth_buffers[handle].setTo(cv::Scalar(std::numeric_limits<float>::max()));
    }


    // Debug flags
    bool draw_wireframes  = (ctx.debug_flag & RHIDebugFlag::FLAG_DRAW_WIREFRAMES) != RHIDebugFlag::FLAG_NONE;
    bool disable_textures = (ctx.debug_flag & RHIDebugFlag::FLAG_DISABLE_TEXTURES) != RHIDebugFlag::FLAG_NONE;
    bool show_uvs         = (ctx.debug_flag & RHIDebugFlag::FLAG_SHOW_UVS) != RHIDebugFlag::FLAG_NONE;

    // --- 3D Rendering --------------------------------------------
    auto mesh_view = registry.view<tf::TFWorldTransformComponent, data::MeshLinkage>();
    auto cam_view = registry.view<camera::CameraComponent, camera::CameraRuntimeComponent>();
    for (auto cam_entity : cam_view) {
        const auto& cam = cam_view.get<camera::CameraComponent>(cam_entity);
        const auto& cam_rt = cam_view.get<camera::CameraRuntimeComponent>(cam_entity);

        // Find the appropriate output matrix for this camera's target FBO
        auto target_it = ctx.render_targets.find(cam.target_fbo);
        if (target_it == ctx.render_targets.end()) {
            continue; // FBO does not exists. Skip
        }
        cv::Mat& render_target = target_it->second;
        cv::Mat& depth_buffer = ctx.depth_buffers[cam.target_fbo];

        // Screen mapping constants based on this specific target's resolution
        float half_w = static_cast<float>(render_target.cols) * 0.5f;
        float half_h = static_cast<float>(render_target.rows) * 0.5f;

        // Mesh drawing
        for (auto physical_entity : mesh_view) {
            const auto& world_tf = mesh_view.get<tf::TFWorldTransformComponent>(physical_entity);
            const auto& linkage = mesh_view.get<data::MeshLinkage>(physical_entity);
            
            // safety checks + visibility check
            if (!registry.valid(linkage.mesh_asset)) continue;
            if (!registry.all_of<data::MeshRuntimeComponent>(linkage.mesh_asset)) continue;
            if ((cam.culling_mask & linkage.layer_mask) == rhi::RenderLayer::LAYER_NONE) continue;

            MeshHandle mesh_handle = registry.get<data::MeshRuntimeComponent>(linkage.mesh_asset).handle;
            if (ctx.meshes.find(mesh_handle) == ctx.meshes.end()) continue;
            const auto& mesh = ctx.meshes[mesh_handle];

            glm::mat4 mvp = cam_rt.view_projection_matrix * world_tf.matrix;

            // Lambda to project a vertex 
            auto project_vertex = [&](const glm::vec3& local_pos) -> glm::vec3 {
                glm::vec4 clip_space = mvp * glm::vec4(local_pos, 1.0f);
                if (clip_space.w <= 0.0001f) return glm::vec3(-1.0f, -1.0f, -1.0f); // Mark invalid
                glm::vec3 ndc = glm::vec3(clip_space) / clip_space.w;
                return glm::vec3((ndc.x + 1.0f) * half_w, (1.0f - ndc.y) * half_h, ndc.z);
            };

            // Point Cloud Rasterization
            if (mesh.topology == data::MeshTopology::POINTS) {
                for (size_t i = 0; i < mesh.positions.size(); ++i) {
                    glm::vec3 p = project_vertex(mesh.positions[i]);
                    if (p.z != -1.0f) {
                        int px = static_cast<int>(p.x);
                        int py = static_cast<int>(p.y);
                        if (px >= 0 && px < render_target.cols && py >= 0 && py < render_target.rows) {
                            if (p.z <= depth_buffer.at<float>(py, px)) {
                                depth_buffer.at<float>(py, px) = p.z;
                                cv::circle(render_target, cv::Point(px, py), 2, cv::Scalar(255, 255, 255), -1);
                            }
                        }
                    }
                }
                continue; // Skip the indexed rendering below
            }
            
            // Lines and Triangles Rasterization
            else if (
                (mesh.topology == data::MeshTopology::TRIANGLES || mesh.topology == data::MeshTopology::LINES) 
                && !mesh.indices.empty()
            ) {
                // Fallback for primitive shapes that lack material definitions
                std::vector<data::MeshMaterial> default_submesh;
                const std::vector<data::MeshMaterial>* active_submeshes = &mesh.materials;

                if (mesh.materials.empty()) {
                    default_submesh.push_back({0, static_cast<uint32_t>(mesh.indices.size()), entt::null});
                    active_submeshes = &default_submesh;
                }

                // Iterate through every material group and draw its specific geometry chunk
                for (const auto& submesh : *active_submeshes) {
                    glm::vec4 mat_base_color(1.0f, 1.0f, 1.0f, 1.0f);
                    cv::Mat* active_albedo = nullptr;
                    
                    // Resolve Material strictly for this submesh
                    if (registry.valid(submesh.material_entity) && registry.all_of<data::MaterialRuntimeComponent>(submesh.material_entity)) {
                        MaterialHandle mat_handle = registry.get<data::MaterialRuntimeComponent>(submesh.material_entity).handle;
                        auto mat_it = ctx.materials.find(mat_handle);
                        if (mat_it != ctx.materials.end()) {
                            const auto& mat_data = mat_it->second; 
                            mat_base_color = mat_data.base_color;
                            
                            if (registry.valid(mat_data.albedo_map) && registry.all_of<data::TextureRuntimeComponent>(mat_data.albedo_map)) {
                                TextureHandle tex_handle = registry.get<data::TextureRuntimeComponent>(mat_data.albedo_map).handle;
                                auto tex_it = ctx.textures.find(tex_handle);
                                if (tex_it != ctx.textures.end()) active_albedo = &tex_it->second;
                            }
                        }
                    }

                    auto get_blended_flat_color = [&](const std::vector<uint32_t>& v_indices) -> cv::Scalar {
                        glm::vec4 v_color(1.0f, 1.0f, 1.0f, 1.0f); 
                        if (!mesh.colors.empty()) {
                            v_color = glm::vec4(0.0f);
                            for (uint32_t idx : v_indices) {
                                if (idx < mesh.colors.size()) {
                                    v_color += glm::vec4(mesh.colors[idx].x, mesh.colors[idx].y, mesh.colors[idx].z, 1.0f);
                                } else {
                                    v_color += glm::vec4(1.0f);
                                }
                            }
                            v_color /= static_cast<float>(v_indices.size()); 
                        }
                        glm::vec4 final_color = mat_base_color * v_color;
                        return cv::Scalar(
                            static_cast<int>(std::clamp(final_color.b * 255.0f, 0.0f, 255.0f)),
                            static_cast<int>(std::clamp(final_color.g * 255.0f, 0.0f, 255.0f)),
                            static_cast<int>(std::clamp(final_color.r * 255.0f, 0.0f, 255.0f))
                        );
                    };
                    // Loop over the indices belonging to this submesh
                    size_t step = (mesh.topology == data::MeshTopology::TRIANGLES) ? 3 : 2;

                    // Calculate safe boundaries for this specific chunk
                    uint32_t start_idx = submesh.index_offset;
                    uint32_t end_idx = std::min(start_idx + submesh.index_count, static_cast<uint32_t>(mesh.indices.size()));

                    for (size_t i = start_idx; i < end_idx; i += step) {
                        
                        // Triangles Pipeline
                        if (step == 3 && i + 2 < end_idx) {
                            uint32_t i0 = mesh.indices[i];
                            uint32_t i1 = mesh.indices[i + 1];
                            uint32_t i2 = mesh.indices[i + 2];

                            glm::vec4 c0 = mvp * glm::vec4(mesh.positions[i0], 1.0f);
                            glm::vec4 c1 = mvp * glm::vec4(mesh.positions[i1], 1.0f);
                            glm::vec4 c2 = mvp * glm::vec4(mesh.positions[i2], 1.0f);

                            if (c0.w <= 0.001f || c1.w <= 0.001f || c2.w <= 0.001f) continue;

                            glm::vec3 ndc0 = glm::vec3(c0) / c0.w;
                            glm::vec3 ndc1 = glm::vec3(c1) / c1.w;
                            glm::vec3 ndc2 = glm::vec3(c2) / c2.w;

                            // Keep these as vec3 to preserve Z-depth!
                            glm::vec3 p0((ndc0.x + 1.0f) * half_w, (1.0f - ndc0.y) * half_h, ndc0.z);
                            glm::vec3 p1((ndc1.x + 1.0f) * half_w, (1.0f - ndc1.y) * half_h, ndc1.z);
                            glm::vec3 p2((ndc2.x + 1.0f) * half_w, (1.0f - ndc2.y) * half_h, ndc2.z);

                            if (draw_wireframes) {
                                cv::Scalar mat_color = get_blended_flat_color({i0, i1, i2});
                                DrawWireframeTriangle(render_target, depth_buffer, p0, p1, p2, mat_color);
                            } else {
                                RasterizeTriangle(
                                    render_target, depth_buffer, 
                                    c0, c1, c2, glm::vec2(p0), glm::vec2(p1), glm::vec2(p2), 
                                    mesh, i0, i1, i2, 
                                    mat_base_color, active_albedo, 
                                    disable_textures, show_uvs
                                );
                            }
                        } 
                        // Lines Pipeline (e.g. Debug Grids and Frustums)
                        else if (step == 2 && i + 1 < end_idx) {
                            glm::vec3 p1 = project_vertex(mesh.positions[mesh.indices[i]]);
                            glm::vec3 p2 = project_vertex(mesh.positions[mesh.indices[i + 1]]);

                            // Only draw if both vertices are in front of the camera
                            if (p1.z != -1.0f && p2.z != -1.0f) {
                                cv::Scalar line_color = get_blended_flat_color({mesh.indices[i], mesh.indices[i+1]});
                                RasterizeLine(render_target, depth_buffer, p1, p2, line_color);
                            }
                        }
                    }
                }
            }
        }
    }


    // --- 2D Rendering --------------------------------------------
    auto ui_view = registry.view<data::TextureLinkage>();
    for (auto ui_entity : ui_view) {
        const auto& linkage = ui_view.get<data::TextureLinkage>(ui_entity);
        
        // safety checks + visibility check
        if (!registry.valid(linkage.texture_asset)) continue;
        if (!registry.all_of<data::TextureRuntimeComponent>(linkage.texture_asset)) continue;
        if ((linkage.layer_mask & rhi::RenderLayer::LAYER_UI) == rhi::RenderLayer::LAYER_NONE) continue;
        
        // Get the hardware texture handle
        rhi::TextureHandle tex_handle = registry.get<data::TextureRuntimeComponent>(linkage.texture_asset).handle;
        
        if (ctx.textures.find(tex_handle) != ctx.textures.end()) {
            const cv::Mat& source_tex = ctx.textures[tex_handle];
            cv::Mat& main_target = ctx.render_targets[TARGET_MAIN];
            
            // Map normalized coordinates [0..1] to actual window pixels
            int px = static_cast<int>(linkage.screen_x * main_target.cols);
            int py = static_cast<int>(linkage.screen_y * main_target.rows);
            int pw = static_cast<int>(linkage.screen_w * main_target.cols);
            int ph = static_cast<int>(linkage.screen_h * main_target.rows);
            
            // Ensure bounds are safe
            if (pw > 0 && ph > 0 && px >= 0 && py >= 0 && px + pw <= main_target.cols && py + ph <= main_target.rows) {
                // Resize and overlay the texture
                cv::Mat resized;
                cv::resize(source_tex, resized, cv::Size(pw, ph));
                resized.copyTo(main_target(cv::Rect(px, py, pw, ph)));
            }
        }
    }



    // Present only the MAIN target to the window
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::imshow(ctx.config.title, ctx.render_targets[TARGET_MAIN]);
        cv::waitKey(1);
    }
}



// --- Render Targets (FBOs) ---------------------------------------
static RenderTargetHandle CreateTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    
    RenderTargetHandle handle = ctx.next_handle++;
    ctx.render_targets[handle]  = cv::Mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    ctx.depth_buffers[handle]   = cv::Mat(height, width, CV_32FC1, cv::Scalar(std::numeric_limits<float>::max()));
    return handle;
}
static void DestroyTarget(entt::registry& registry, RenderTargetHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    if (handle == TARGET_MAIN) return; // Never destroy the main screen buffer directly
    
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.render_targets.erase(handle);
    ctx.depth_buffers.erase(handle);
}



// --- Textures ----------------------------------------------------
static void UpdateTexture(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.textures.find(handle) == ctx.textures.end() || image_data.data.empty()) return;

    int type = (image_data.channels == data::ImageChannelLayout::CH_4) ? CV_8UC4 : CV_8UC3;
    cv::Mat incoming(image_data.height, image_data.width, type, (void*)image_data.data.data());
    incoming.copyTo(ctx.textures[handle]);
}
static TextureHandle CreateTexture(entt::registry& registry, const data::ImageData& image_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    
    TextureHandle handle = ctx.next_tex_handle++;
    
    // Allocate the empty matrix using the correct type
    int type = (image_data.channels == data::ImageChannelLayout::CH_4) ? CV_8UC4 : CV_8UC3;
    ctx.textures[handle] = cv::Mat(image_data.height, image_data.width, type, cv::Scalar(0,0,0));
    
    // Push the initial bytes 
    UpdateTexture(registry, handle, image_data);
    return handle;
}
static void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.textures.erase(handle); // Safely frees the OpenCV cv::Mat memory
}



// --- Meshes ------------------------------------------------------
static void UpdateMesh(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.meshes.find(handle) != ctx.meshes.end()) {
        ctx.meshes[handle] = mesh_data; // Update the cached geometry
    }
}

static MeshHandle CreateMesh(entt::registry& registry, const data::MeshData& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    MeshHandle handle = ctx.next_mesh_handle++;
    
    ctx.meshes[handle] = mesh_data; // Cache the geometry
    return handle;
}

static void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.meshes.erase(handle);
}



// --- Materials ---------------------------------------------------
static void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.materials.find(handle) != ctx.materials.end()) {
        ctx.materials[handle] = material_data;
    }
}
static MaterialHandle CreateMaterial(entt::registry& registry, const data::MaterialData& material_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    MaterialHandle handle = ctx.next_mat_handle++;
    
    ctx.materials[handle] = material_data;
    return handle;
}
static void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.materials.erase(handle);
}


// --- Retrieve Rendered Data --------------------------------------
static data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();

    data::ImageData img;
    if (ctx.render_targets.find(handle) == ctx.render_targets.end()) return img;

    const cv::Mat& mat = ctx.render_targets[handle];
    
    img.width = mat.cols;
    img.height = mat.rows;
    img.channels = data::ImageChannelLayout::CH_3;
    img.data_type = data::ImageDataType::UINT8;
    img.color_layout = data::ImageColorLayout::BGR;

    size_t total_bytes = mat.total() * mat.elemSize();
    img.data.assign(mat.datastart, mat.datastart + total_bytes);

    return img;
}


// --- Debugging ---------------------------------------------------
static void SetDebugFlag(entt::registry& registry, RHIDebugFlag flag, bool enable) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();

    if (enable) ctx.debug_flag |= flag;
    else ctx.debug_flag &= ~flag;
}
static RHIDebugFlag GetActiveDebugFlags(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    return registry.ctx().get<OpenCVContext>().debug_flag;
}


// --- Creation ----------------------------------------------------
RHIBackend CreateBackend() {
    RHIBackend backend;
    backend.type                = RHIBackendType::OPENCV;

    backend.Initialize          = Initialize;
    backend.Shutdown            = Shutdown;
    backend.RenderFrame         = RenderFrame;

    backend.CreateRenderTarget  = CreateTarget;
    backend.DestroyRenderTarget = DestroyTarget;

    backend.CreateTexture       = CreateTexture;
    backend.UpdateTexture       = UpdateTexture;
    backend.DestroyTexture      = DestroyTexture;

    backend.CreateMesh          = CreateMesh;
    backend.UpdateMesh          = UpdateMesh;
    backend.DestroyMesh         = DestroyMesh;

    backend.CreateMaterial      = CreateMaterial;
    backend.UpdateMaterial      = UpdateMaterial;
    backend.DestroyMaterial     = DestroyMaterial;

    backend.GetTargetImage      = GetTargetImage;
    
    backend.SetDebugFlag        = SetDebugFlag;
    backend.GetActiveDebugFlags = GetActiveDebugFlags;
    
    return backend;
}


} // namespace rrl::rhi::opencv 
