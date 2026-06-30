// RRL/src/rhi/opencv/OpenCVRenderer.cpp

#include "RRL/data/ImageData.hpp"
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
#include "RRL/rhi/software/SWRRenderer.hpp"


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
    
    // Core Engine Data Buffers (Targeted by SWRRenderer)
    std::unordered_map<RenderTargetHandle, data::ImageData> render_targets;
    std::unordered_map<RenderTargetHandle, data::ImageData> depth_buffers;
    std::unordered_map<RenderTargetHandle, software::ColorFormatCache> rt_formats;

    std::unordered_map<TextureHandle, data::ImageData> textures;
    std::unordered_map<TextureHandle, software::ColorFormatCache> tex_formats;

    // OpenCV Wrappers (no copy)
    std::unordered_map<RenderTargetHandle, cv::Mat> cv_targets;
    std::unordered_map<TextureHandle, cv::Mat> cv_textures;

    // SIMD Assets
    std::unordered_map<MeshHandle, software::SWRMesh> meshes; 
    std::unordered_map<MaterialHandle, data::MaterialData> materials;

    // Vertex Shader Working Buffer 
    software::SWRVertexBuffer working_vertex_buffer;

    RenderTargetHandle next_handle  { 1 }; // 0 is reserved for TARGET_MAIN
    TextureHandle next_tex_handle   { 1 };
    MeshHandle next_mesh_handle     { 1 };
    MaterialHandle next_mat_handle  { 1 };
    
    RHIDebugFlag debug_flag { RHIDebugFlag::FLAG_NONE };
};



// --- Lifecycle ---------------------------------------------------
static bool Initialize(entt::registry& registry, const RHIConfig& config) {
    auto& ctx = registry.ctx().emplace<OpenCVContext>();
    ctx.config = config;

    // Create TARGET_MAIN
    data::ImageData rt;
    rt.width = config.width; 
    rt.height = config.height; 
    rt.data_type = data::ImageDataType::UINT8;
    rt.channels = data::ImageChannelLayout::CH_3;
    rt.color_layout = data::ImageColorLayout::BGR;
    rt.data.resize(config.width * config.height * 3, 30);
    ctx.render_targets[TARGET_MAIN] = std::move(rt);
    ctx.rt_formats[TARGET_MAIN]     = software::GetColorFormatCache(rt.color_layout, rt.channels);
    RRL_ASSERT(rt.IsImageModelValid(), "[OpernCV RHI] Error creating the TARGET_MAIN image");
    
    // Create Depth buffer
    data::ImageData depth;
    depth.width = config.width; 
    depth.height = config.height; 
    depth.data_type = data::ImageDataType::FLOAT32;
    depth.channels = data::ImageChannelLayout::CH_1; 
    depth.color_layout = data::ImageColorLayout::NONE; 
    depth.data.resize(config.width * config.height * sizeof(float));
    ctx.depth_buffers[TARGET_MAIN] = std::move(depth);
    RRL_ASSERT(depth.IsImageModelValid(), "[OpernCV RHI] Error creating the TARGET_MAIN depth buffer");

    // OpenCV wrapper for window presentation
    ctx.cv_targets[TARGET_MAIN] = cv::Mat(config.height, config.width, CV_8UC3, ctx.render_targets[TARGET_MAIN].data.data());
    if (config.mode == RHIRenderingMode::WINDOW) {
        cv::namedWindow(config.title, cv::WINDOW_AUTOSIZE);
    }
    LOG_WARN("[OpenCV RHI] Software rendering pipeline. Using SIMD AoSoA for accelerated graphics.");
    return true;
}
static void Shutdown(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::destroyWindow(ctx.config.title);
    }
    registry.ctx().erase<OpenCVContext>();
}
static void RenderFrame(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();

    // Clear all active targets using OpenCV wrappers
    for (auto& [handle, target] : ctx.render_targets) {
        ctx.cv_targets[handle].setTo(cv::Scalar(30, 30, 30));
        
        cv::Mat depth_wrapper(target.height, target.width, CV_32FC1, ctx.depth_buffers[handle].data.data());
        depth_wrapper.setTo(cv::Scalar(std::numeric_limits<float>::max()));
    }

    // Resolve Debug Flags
    bool draw_wireframes         = (ctx.debug_flag & RHIDebugFlag::FLAG_DRAW_WIREFRAMES) != RHIDebugFlag::FLAG_NONE;
    bool disable_textures        = (ctx.debug_flag & RHIDebugFlag::FLAG_DISABLE_TEXTURES) != RHIDebugFlag::FLAG_NONE;
    bool show_uvs                = (ctx.debug_flag & RHIDebugFlag::FLAG_SHOW_UVS) != RHIDebugFlag::FLAG_NONE;
    bool runtime_affine_override = (ctx.debug_flag & RHIDebugFlag::FLAG_AFFINE_INTERPOLATION) != RHIDebugFlag::FLAG_NONE;

    // 3D Rendering (loop for each camera and for each mesh (sub mesh material rendering))
    auto mesh_view = registry.view<tf::TFWorldTransformComponent, data::MeshLinkage>();
    auto cam_view = registry.view<camera::CameraComponent, camera::CameraRuntimeComponent>();
    for (auto cam_entity : cam_view) {
        const auto& cam = cam_view.get<camera::CameraComponent>(cam_entity);
        const auto& cam_rt = cam_view.get<camera::CameraRuntimeComponent>(cam_entity);

        auto target_it = ctx.render_targets.find(cam.target_fbo);
        if (target_it == ctx.render_targets.end()) continue;
        
        data::ImageData& render_target = target_it->second;
        data::ImageData& depth_buffer = ctx.depth_buffers[cam.target_fbo];
        cv::Mat& cv_render_target = ctx.cv_targets[cam.target_fbo]; // For point circles

        float half_w = static_cast<float>(render_target.width) * 0.5f;
        float half_h = static_cast<float>(render_target.height) * 0.5f;

        for (auto physical_entity : mesh_view) {
            const auto& world_tf = mesh_view.get<tf::TFWorldTransformComponent>(physical_entity);
            const auto& linkage = mesh_view.get<data::MeshLinkage>(physical_entity);
            
            if (!registry.valid(linkage.mesh_asset)) continue;
            if (!registry.all_of<data::MeshRuntimeComponent>(linkage.mesh_asset)) continue;
            if ((cam.culling_mask & linkage.layer_mask) == rhi::RenderLayer::LAYER_NONE) continue;

            MeshHandle mesh_handle = registry.get<data::MeshRuntimeComponent>(linkage.mesh_asset).handle;
            if (ctx.meshes.find(mesh_handle) == ctx.meshes.end()) continue;
            const auto& mesh = ctx.meshes[mesh_handle];

            glm::mat4 mvp = cam_rt.view_projection_matrix * world_tf.matrix;

            // Vertex Shader (vertex projection)
            software::SWRVertexShader(mesh, mvp, ctx.working_vertex_buffer);

            // Point Cloud Rasterization
            if (mesh.topology == data::MeshTopology::POINTS) {
                for (size_t i = 0; i < mesh.active_vertex_count; ++i) {
                    const glm::vec4& ndc = ctx.working_vertex_buffer.ndc_positions[i];
                    
                    // Frustum Culled
                    if (ndc.w <= 0.0001f) continue; 
                    
                    // Viewport Transform
                    int px = static_cast<int>((ndc.x + 1.0f) * half_w);
                    int py = static_cast<int>((1.0f - ndc.y) * half_h);
                    
                    if (px >= 0 && px < render_target.width && py >= 0 && py < render_target.height) {
                        float* d_ptr = reinterpret_cast<float*>(depth_buffer.data.data());
                        if (ndc.z <= d_ptr[py * render_target.width + px]) {
                            d_ptr[py * render_target.width + px] = ndc.z;
                            // OpenCV handles drawing the tiny circle perfectly here
                            cv::circle(cv_render_target, cv::Point(px, py), 2, cv::Scalar(255, 255, 255), -1);
                        }
                    }
                }
                continue; 
            }
            
            // Triangles and Lines Rasterization
            else if (!mesh.indices.empty()) {
                std::vector<data::MeshMaterial> default_submesh;
                const std::vector<data::MeshMaterial>* active_submeshes = &mesh.materials;

                if (mesh.materials.empty()) {
                    default_submesh.push_back({0, static_cast<uint32_t>(mesh.indices.size()), entt::null});
                    active_submeshes = &default_submesh;
                }

                for (const auto& submesh : *active_submeshes) {
                    glm::vec4 mat_base_color(1.0f, 1.0f, 1.0f, 1.0f);
                    const data::ImageData* active_albedo = nullptr;
                    software::ColorFormatCache tex_format{};
                    
                    if (registry.valid(submesh.material_entity) && registry.all_of<data::MaterialRuntimeComponent>(submesh.material_entity)) {
                        MaterialHandle mat_handle = registry.get<data::MaterialRuntimeComponent>(submesh.material_entity).handle;
                        auto mat_it = ctx.materials.find(mat_handle);
                        
                        if (mat_it != ctx.materials.end()) {
                            mat_base_color = mat_it->second.base_color;
                            
                            if (registry.valid(mat_it->second.albedo_map) && registry.all_of<data::TextureRuntimeComponent>(mat_it->second.albedo_map)) {
                                TextureHandle tex_handle = registry.get<data::TextureRuntimeComponent>(mat_it->second.albedo_map).handle;
                                if (ctx.textures.find(tex_handle) != ctx.textures.end()) {
                                    active_albedo = &ctx.textures[tex_handle];
                                    tex_format = ctx.tex_formats[tex_handle];
                                }
                            }
                        }
                    }

                    software::SWRRender3DMesh(
                        render_target, depth_buffer, mesh, ctx.working_vertex_buffer,
                        submesh.index_offset, submesh.index_count, 
                        mat_base_color, active_albedo, 
                        ctx.rt_formats[cam.target_fbo], tex_format,
                        disable_textures, show_uvs, runtime_affine_override, draw_wireframes
                    );
                }
            }
        }
    }

    // 2D Rendering (UI Layer)
    auto ui_view = registry.view<data::TextureLinkage>();
    for (auto ui_entity : ui_view) {
        const auto& linkage = ui_view.get<data::TextureLinkage>(ui_entity);
        
        if (!registry.valid(linkage.texture_asset)) continue;
        if (!registry.all_of<data::TextureRuntimeComponent>(linkage.texture_asset)) continue;
        if ((linkage.layer_mask & rhi::RenderLayer::LAYER_UI) == rhi::RenderLayer::LAYER_NONE) continue;
        
        rhi::TextureHandle tex_handle = registry.get<data::TextureRuntimeComponent>(linkage.texture_asset).handle;
        
        if (ctx.cv_textures.find(tex_handle) != ctx.cv_textures.end()) {
            const cv::Mat& source_tex = ctx.cv_textures[tex_handle];
            cv::Mat& main_target = ctx.cv_targets[TARGET_MAIN];
            
            int px = static_cast<int>(linkage.screen_x * main_target.cols);
            int py = static_cast<int>(linkage.screen_y * main_target.rows);
            int pw = static_cast<int>(linkage.screen_w * main_target.cols);
            int ph = static_cast<int>(linkage.screen_h * main_target.rows);
            
            if (pw > 0 && ph > 0 && px >= 0 && py >= 0 && px + pw <= main_target.cols && py + ph <= main_target.rows) {
                cv::Mat resized;
                cv::resize(source_tex, resized, cv::Size(pw, ph));
                resized.copyTo(main_target(cv::Rect(px, py, pw, ph)));
            }
        }
    }

    // Present only the MAIN target to the window
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::imshow(ctx.config.title, ctx.cv_targets[TARGET_MAIN]);
        cv::waitKey(1);
    }
}



// --- Render Targets (FBOs) ---------------------------------------
static RenderTargetHandle CreateTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    
    RenderTargetHandle handle = ctx.next_handle++;
    
    data::ImageData rt;
    rt.width = width; rt.height = height; 
    rt.data_type = data::ImageDataType::UINT8;
    rt.channels = data::ImageChannelLayout::CH_3;
    rt.color_layout = data::ImageColorLayout::BGR;
    rt.data.resize(width * height * 3, 0);
    ctx.render_targets[handle]  = std::move(rt);
    ctx.rt_formats[handle]      = software::GetColorFormatCache(rt.color_layout, rt.channels);
    RRL_ASSERT(rt.IsImageModelValid(), "[OpernCV RHI] Error creating the '{}' FBO target image");
    
    data::ImageData depth;
    depth.width = width; depth.height = height; 
    depth.data_type = data::ImageDataType::FLOAT32;
    depth.channels = data::ImageChannelLayout::CH_1;
    rt.color_layout = data::ImageColorLayout::NONE;
    depth.data.resize(width * height * sizeof(float));
    ctx.depth_buffers[handle] = std::move(depth);
    RRL_ASSERT(depth.IsImageModelValid(), "[OpernCV RHI] Error creating the '{}' FBO target depth buffer");

    // OpenCV wrappers
    ctx.cv_targets[handle] = cv::Mat(height, width, CV_8UC3, ctx.render_targets[handle].data.data());
    return handle;
}
static void DestroyTarget(entt::registry& registry, RenderTargetHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    if (handle == TARGET_MAIN) return; 
    
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.render_targets.erase(handle);
    ctx.depth_buffers.erase(handle);
    ctx.rt_formats.erase(handle);
    ctx.cv_targets.erase(handle);
}



// --- Textures ----------------------------------------------------
static void UpdateTexture(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    RRL_ASSERT(image_data.IsImageModelValid(), "OpenCVRenderer received an invalid image model for texture updating!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.textures.find(handle) == ctx.textures.end() || image_data.data.empty()) return;

    ctx.textures[handle] = image_data;
    ctx.tex_formats[handle] = software::GetColorFormatCache(image_data.color_layout, image_data.channels);

    // Rebind CV wrapper 
    int type = (image_data.channels == data::ImageChannelLayout::CH_4) ? CV_8UC4 : CV_8UC3;
    ctx.cv_textures[handle] = cv::Mat(image_data.height, image_data.width, type, ctx.textures[handle].data.data());
}
static TextureHandle CreateTexture(entt::registry& registry, const data::ImageData& image_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    RRL_ASSERT(image_data.IsImageModelValid(), "OpenCVRenderer received an invalid image model for texture creation!");

    auto& ctx = registry.ctx().get<OpenCVContext>();
    
    TextureHandle handle = ctx.next_tex_handle++;
    ctx.textures[handle] = data::ImageData{};
    UpdateTexture(registry, handle, image_data);
    return handle;
}
static void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.textures.erase(handle);
    ctx.tex_formats.erase(handle);
    ctx.cv_textures.erase(handle);
}



// --- Meshes ------------------------------------------------------
static void UpdateMesh(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.meshes.find(handle) != ctx.meshes.end()) {
        software::LoadMeshDataIntoSWRMesh(mesh_data, ctx.meshes[handle]);
    }
}
static MeshHandle CreateMesh(entt::registry& registry, const data::MeshData& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<OpenCVContext>(), "OpenCVRenderer not initialized!");
    auto& ctx = registry.ctx().get<OpenCVContext>();
    MeshHandle handle = ctx.next_mesh_handle++;
    
    software::LoadMeshDataIntoSWRMesh(mesh_data, ctx.meshes[handle]);
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

    // Return a copy of the raw ImageData 
    img = ctx.render_targets[handle];
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
