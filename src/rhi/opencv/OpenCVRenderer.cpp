// RRL/src/rhi/opencv/OpenCVRenderer.cpp

#include "RRL/data/MeshData.hpp"
#include "RRL/rhi/RHIAPI.hpp"
#include "entt/entity/entity.hpp"
#ifndef  RRL_RHI_OPENCV
    #error "OpenCVRenderer.hpp file must be compiled with OpenCV support."
#endif

#include "RRL/camera/CameraComponents.hpp"
#include "RRL/tf/TFComponents.hpp"

#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MaterialComponents.hpp"

#include "RRL/rhi/RHIBackend.hpp"


#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <FLogging/FLogging.hpp>

#include <glm/glm.hpp>
#include <unordered_map>


namespace rrl::rhi::opencv {


/**
 * @brief Holds the running OpenCV rendering backend context.
 */
struct OpenCVContext {
    RHIConfig config;
    std::unordered_map<RenderTargetHandle, cv::Mat> render_targets;
    std::unordered_map<TextureHandle, cv::Mat> textures;
    std::unordered_map<MeshHandle, data::MeshData> meshes;
    std::unordered_map<MaterialHandle, data::MaterialData> materials;

    RenderTargetHandle next_handle  { 1 }; // 0 is reserved for TARGET_MAIN
    TextureHandle next_tex_handle   { 1 };
    MeshHandle next_mesh_handle     { 1 };
    MaterialHandle next_mat_handle  { 1 };
};


// --- Lifecycle ---------------------------------------------------
static bool Initialize(entt::registry& registry, const RHIConfig& config) {
    // Inject the context into the registry
    auto& ctx = registry.ctx().emplace<OpenCVContext>();
    ctx.config = config;

    // Implicitly create TARGET_MAIN
    ctx.render_targets[TARGET_MAIN] = cv::Mat(config.height, config.width, CV_8UC3, cv::Scalar(30, 30, 30));

    if (config.mode == RHIRenderingMode::WINDOW) {
        cv::namedWindow(config.title, cv::WINDOW_AUTOSIZE);
    }
    
    LOG_WARN("[OpenCV RHI] CPU-only mode. Expect deep-copy memory overhead during asset synchronization to mimic GPU VRAM uploads.");
    return true;
}
static void Shutdown(entt::registry& registry) {
    // If the context doesn't exist, there's nothing to clean
    if (!registry.ctx().contains<OpenCVContext>()) return;

    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::destroyWindow(ctx.config.title);
    }
    
    // Remove the context entirely, destroying all cv::Mat buffers automatically
    registry.ctx().erase<OpenCVContext>();
}
static void RenderFrame(entt::registry& registry) {
    auto& ctx = registry.ctx().get<OpenCVContext>();

    // Clear all active targets
    for (auto& [handle, mat] : ctx.render_targets) {
        mat.setTo(cv::Scalar(30, 30, 30));
    }



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

        // Screen mapping constants based on this specific target's resolution
        float half_w = static_cast<float>(render_target.cols) * 0.5f;
        float half_h = static_cast<float>(render_target.rows) * 0.5f;

        // Mesh drawing
        for (auto physical_entity : mesh_view) {
            const auto& world_tf = mesh_view.get<tf::TFWorldTransformComponent>(physical_entity);
            const auto& linkage = mesh_view.get<data::MeshLinkage>(physical_entity);
            
            // Obtain actual mesh data
            if (!registry.valid(linkage.mesh_asset)) continue;
            if (!registry.all_of<data::MeshRuntimeComponent>(linkage.mesh_asset)) continue;
            MeshHandle mesh_handle = registry.get<data::MeshRuntimeComponent>(linkage.mesh_asset).handle;
            if (ctx.meshes.find(mesh_handle) == ctx.meshes.end()) continue;
            const auto& mesh = ctx.meshes[mesh_handle];

            // Pre-compute the final Model-View-Projection (MVP) matrix
            glm::mat4 mvp = cam_rt.view_projection_matrix * world_tf.matrix;

            

            // Lambda to project a 3D point in Local Coordinate System (LCS) to 2D pixel coordinates
            auto project_vertex = [&](const glm::vec3& local_pos) -> cv::Point {
                glm::vec4 clip_space = mvp * glm::vec4(local_pos, 1.0f);
                
                // Near plane clipping guard (if W <= 0, it's behind or right on the camera plane)
                if (clip_space.w <= 0.0001f) {
                    return cv::Point(-1, -1);
                }
                
                // Perspective Divide -> NDC Space
                glm::vec3 ndc = glm::vec3(clip_space) / clip_space.w;

                // Viewport Transform (NDC [-1, 1] to Pixels [0, width/height])
                int px = static_cast<int>((ndc.x + 1.0f) * half_w);
                // Map NDC +1 (Top) to Pixel 0 (Top)
                // int py = static_cast<int>((ndc.y + 1.0f) * half_h);
                int py = static_cast<int>((1.0f - ndc.y) * half_h);

                return cv::Point(px, py);
            };

            
            // Resolve material
            glm::vec4 mat_base_color(1.0f, 1.0f, 1.0f, 1.0f);
            if (!mesh.materials.empty()) {
                entt::entity mat_entity = mesh.materials[0].material_entity;
                if (mat_entity != entt::null && registry.valid(mat_entity) && registry.all_of<data::MaterialRuntimeComponent>(mat_entity)) {
                    MaterialHandle mat_handle = registry.get<data::MaterialRuntimeComponent>(mat_entity).handle;
                    if (ctx.materials.find(mat_handle) != ctx.materials.end()) {
                        mat_base_color = ctx.materials[mat_handle].base_color;
                    }
                }
            }

            // Lambda to blend colors
            auto get_blended_color = [&](const std::vector<uint32_t>& v_indices) -> cv::Scalar {
                glm::vec4 v_color(1.0f, 1.0f, 1.0f, 1.0f); // Default white vertex
                
                if (!mesh.colors.empty()) {
                    v_color = glm::vec4(0.0f);
                    for (uint32_t idx : v_indices) {
                        if (idx < mesh.colors.size()) {
                            v_color += glm::vec4(mesh.colors[idx].x, mesh.colors[idx].y, mesh.colors[idx].z, 1.0f);
                        } else {
                            v_color += glm::vec4(1.0f);
                        }
                    }
                    v_color /= static_cast<float>(v_indices.size()); // Average across the face
                }
                glm::vec4 final_color = mat_base_color * v_color;
                return cv::Scalar(
                    static_cast<int>(std::clamp(final_color.b * 255.0f, 0.0f, 255.0f)), // B
                    static_cast<int>(std::clamp(final_color.g * 255.0f, 0.0f, 255.0f)), // G
                    static_cast<int>(std::clamp(final_color.r * 255.0f, 0.0f, 255.0f))  // R
                );
            };

            
            // Point Cloud Rasterization
            if (mesh.topology == data::MeshTopology::POINTS) {
                for (size_t i = 0; i < mesh.positions.size(); ++i) {
                    cv::Point p = project_vertex(mesh.positions[i]);
                    
                    if (p.x >= 0 && p.x < render_target.cols && p.y >= 0 && p.y < render_target.rows) {
                        cv::Scalar final_color = get_blended_color({ static_cast<uint32_t>(i) });
                        cv::circle(render_target, p, 2, final_color, -1); // Radius 2 for better visibility
                    }
                }
            } 
            // Triangles and Lines Rasterization
            else if ((mesh.topology == data::MeshTopology::TRIANGLES || mesh.topology == data::MeshTopology::LINES) 
                     && !mesh.indices.empty()) 
            {
                size_t step = (mesh.topology == data::MeshTopology::TRIANGLES) ? 3 : 2;
                
                for (size_t i = 0; i < mesh.indices.size(); i += step) {
                    
                    // Solid Triangles
                    if (step == 3 && i + 2 < mesh.indices.size()) {
                        cv::Point p1 = project_vertex(mesh.positions[mesh.indices[i]]);
                        cv::Point p2 = project_vertex(mesh.positions[mesh.indices[i + 1]]);
                        cv::Point p3 = project_vertex(mesh.positions[mesh.indices[i + 2]]);

                        if (p1.x != -1 && p2.x != -1 && p3.x != -1) {
                            cv::Scalar face_color = get_blended_color({mesh.indices[i], mesh.indices[i+1], mesh.indices[i+2]});
                            
                            // Fill the solid polygon
                            std::vector<cv::Point> pts = {p1, p2, p3};
                            cv::fillConvexPoly(render_target, pts.data(), 3, face_color);

                            // (Because OpenCV lacks a Z-buffer, solid objects look like flat blobs without edges)
                            cv::Scalar edge_color(face_color[0] * 0.7, face_color[1] * 0.7, face_color[2] * 0.7);
                            cv::polylines(render_target, pts, true, edge_color, 1);
                        }
                    } 
                    // Lines
                    else if (step == 2 && i + 1 < mesh.indices.size()) {
                        cv::Point p1 = project_vertex(mesh.positions[mesh.indices[i]]);
                        cv::Point p2 = project_vertex(mesh.positions[mesh.indices[i + 1]]);

                        if (p1.x != -1 && p2.x != -1) {
                            cv::Scalar line_color = get_blended_color({mesh.indices[i], mesh.indices[i+1]});
                            cv::line(render_target, p1, p2, line_color, 2);
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
        
        if (!registry.valid(linkage.texture_asset)) continue;
        if (!registry.all_of<data::TextureRuntimeComponent>(linkage.texture_asset)) continue;
        
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
    auto& ctx = registry.ctx().get<OpenCVContext>();
    
    RenderTargetHandle handle = ctx.next_handle++;
    ctx.render_targets[handle] = cv::Mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    return handle;
}
static void DestroyTarget(entt::registry& registry, RenderTargetHandle handle) {
    if (handle == TARGET_MAIN) return; // Never destroy the main screen buffer directly
    
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.render_targets.erase(handle);
}



// --- Textures ----------------------------------------------------
static void UpdateTexture(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.textures.find(handle) == ctx.textures.end() || image_data.data.empty()) return;

    int type = (image_data.channels == data::ImageChannelLayout::CH_4) ? CV_8UC4 : CV_8UC3;
    cv::Mat incoming(image_data.height, image_data.width, type, (void*)image_data.data.data());
    incoming.copyTo(ctx.textures[handle]);
}
static TextureHandle CreateTexture(entt::registry& registry, const data::ImageData& image_data) {
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
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.textures.erase(handle); // Safely frees the OpenCV cv::Mat memory
}



// --- Meshes ------------------------------------------------------
static void UpdateMesh(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.meshes.find(handle) != ctx.meshes.end()) {
        ctx.meshes[handle] = mesh_data; // Update the cached geometry
    }
}

static MeshHandle CreateMesh(entt::registry& registry, const data::MeshData& mesh_data) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    MeshHandle handle = ctx.next_mesh_handle++;
    
    ctx.meshes[handle] = mesh_data; // Cache the geometry
    return handle;
}

static void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.meshes.erase(handle);
}



// --- Materials ---------------------------------------------------
static void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.materials.find(handle) != ctx.materials.end()) {
        ctx.materials[handle] = material_data;
    }
}
static MaterialHandle CreateMaterial(entt::registry& registry, const data::MaterialData& material_data) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    MaterialHandle handle = ctx.next_mat_handle++;
    
    ctx.materials[handle] = material_data;
    return handle;
}
static void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.materials.erase(handle);
}


// --- Retrieve Rendered Data --------------------------------------
static data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    data::ImageData img;
    auto& ctx = registry.ctx().get<OpenCVContext>();

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
    return backend;
}


} // namespace rrl::rhi::opencv 
