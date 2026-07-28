// RRL/src/rhi/software/SoftwareRenderer.cpp


// Runtime components
#include "RRL/camera/CameraComponents.hpp"
#include "RRL/tf/TFComponents.hpp"
#include "RRL/asset/TextureComponents.hpp"
#include "RRL/asset/MeshComponents.hpp"
#include "RRL/asset/MaterialComponents.hpp"


#include "RRL/asset/ImageAsset.hpp"
#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/rhi/software/SWRRasterizer.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"
#include "entt/entity/entity.hpp"

#include <unordered_map>
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>


// OpenCV Window Presentation Support
#ifdef RRL_BUILD_WINDOW_OPENCV 
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#endif

#ifdef RRL_BUILD_WINDOW_GLFW
#include <glad/glad.h> // GLFW windowing ensures OpenGL is available 
#include <GLFW/glfw3.h>
#endif

namespace rrl::rhi::software {

constexpr size_t MAX_COLOR_ATTACHMENTS = static_cast<size_t>(rhi::RHIRenderAttachmentSemantic::_COUNT);

/**
 * @brief Represents a single color attachment slice
 */
struct SWRenderAttachment {
    TextureHandle handle { rhi::BACKEND_TEXTURE_NULL };
    uint32_t array_idx { 0 };
    size_t slice_byte_offset { 0 }; // memory offset for Texture Arrays
};

/**
 * @brief Represents a physical FBO mapping to underlying TextureHandles
 */
struct SWRenderTarget {
    uint32_t width { 0 }, height { 0 };
    
    //Mapping layout_location -> Attachment
    std::array<SWRenderAttachment, MAX_COLOR_ATTACHMENTS> color_attachments;
    
    // Depth buffer (Z-buffer). All targets have one
    std::vector<float> depth_buffer;
};

/**
 * @brief Holds the running Software rendering backend context.
 */
struct SoftwareContext {
    uint32_t render_width;
    uint32_t render_height;
    const RHIWindow* active_window { nullptr };
    
    // Core Engine Data Buffers 
    std::unordered_map<RenderTargetHandle, SWRenderTarget>          render_targets;
    std::unordered_map<TextureHandle, rrl::asset::ImageAsset>       textures;
    std::unordered_map<TextureHandle, software::ColorFormatCache>   tex_formats;

    // Assets
    std::unordered_map<MeshHandle, software::SWRMesh>               meshes; 
    std::unordered_map<MaterialHandle, rrl::asset::MaterialAsset>   materials;

    // Vertex Shader Working Buffer 
    software::SWRVertexBuffer working_vertex_buffer;
    
    // Scene Environment
    PhysicalEnvironmentDescriptor environment_desc;


    RenderTargetHandle next_handle  { 1 }; // 0 is reserved for TARGET_MAIN
    TextureHandle next_tex_handle   { 1 };
    MeshHandle next_mesh_handle     { 1 };
    MaterialHandle next_mat_handle  { 1 };
    
    RHIDebugFlag debug_flag { RHIDebugFlag::FLAG_NONE };
    

    // GPFW presentation requires some OpenGL state variables
    #ifdef RRL_BUILD_WINDOW_GLFW
    bool gl_pointers_loaded { false };
    uint32_t presentation_fbo { 0 };
    uint32_t presentation_tex { 0 };
    uint32_t last_blit_w { 0 };
    uint32_t last_blit_h { 0 };
    #endif
};



// --- Lifecycle ---------------------------------------------------
static bool Initialize(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window) {
    RRL_ASSERT(window != nullptr, "[Software RHI] Received a null window ptr");
    auto& ctx = registry.ctx().emplace<SoftwareContext>();
    ctx.render_width = render_width;
    ctx.render_height = render_height;
    ctx.active_window = window;

    // Allocate physical texture handles for TARGET_MAIN
    TextureHandle main_color = ctx.next_tex_handle++;
    TextureHandle main_depth = ctx.next_tex_handle++;

    // Create TARGET_MAIN Color Texture
    rrl::asset::ImageAsset rt;
    rt.width            = render_width; 
    rt.height           = render_height; 
    rt.data_type        = rrl::asset::ImageAssetType::UINT8;
    rt.channels         = rrl::asset::ImageChannelLayout::CH_3;
    rt.color_layout     = rrl::asset::ImageColorLayout::BGR;
    rt.data.resize(render_width * render_height * 3, 30);
    RRL_ASSERT(rt.IsValid(), "[Software RHI] Error creating TARGET_MAIN image");
    ctx.textures[main_color]    = std::move(rt);
    ctx.tex_formats[main_color] = software::GetColorFormatCache(ctx.textures[main_color].color_layout, ctx.textures[main_color].channels);
    
    // Create TARGET_MAIN FBO with internal Z-buffer
    SWRenderTarget target_main;
    target_main.width = render_width;
    target_main.height = render_height;
    target_main.color_attachments[0] = { main_color, 0, 0 }; // Map to layout(location = 0)
    target_main.depth_buffer.resize(render_width * render_height, std::numeric_limits<float>::max());
    ctx.render_targets[BACKEND_TARGET_MAIN] = std::move(target_main);


    LOG_WARN("[Software RHI] Initialized successfully. Using SIMD AoSoA for accelerated graphics.");
    return true;
}
static void Shutdown(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    
    #ifdef RRL_BUILD_WINDOW_GLFW
    if (ctx.presentation_tex != 0 || ctx.presentation_fbo != 0) {
        if (ctx.active_window && ctx.active_window->type == RHIWindowType::GLFW) {
            GLFWwindow* gl_window = static_cast<GLFWwindow*>(ctx.active_window->native_handle);
            if (gl_window) {
                glfwMakeContextCurrent(gl_window);
                if (ctx.presentation_tex) glDeleteTextures(1, &ctx.presentation_tex);
                if (ctx.presentation_fbo) glDeleteFramebuffers(1, &ctx.presentation_fbo);
                glfwMakeContextCurrent(nullptr);
            }
        }
    }
    #endif

    registry.ctx().erase<SoftwareContext>();
}
static void RenderFrame(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();

    // Clear all active targets in RAM
    for (auto& [handle, target] : ctx.render_targets) {
        // Clear Internal Depth Buffer
        std::fill(target.depth_buffer.begin(), target.depth_buffer.end(), std::numeric_limits<float>::max());
        
        // Clear active Color Attachments
        for (const auto& attach : target.color_attachments) {
            if (attach.handle != rhi::BACKEND_TEXTURE_NULL) {
                auto& img = ctx.textures[attach.handle];
                std::fill(img.data.begin(), img.data.end(), 30);
            }
        }
    }

    // Resolve Debug Flags
    bool draw_wireframes         = (ctx.debug_flag & RHIDebugFlag::FLAG_DRAW_WIREFRAMES) != RHIDebugFlag::FLAG_NONE;
    bool disable_textures        = (ctx.debug_flag & RHIDebugFlag::FLAG_DISABLE_TEXTURES) != RHIDebugFlag::FLAG_NONE;
    bool show_uvs                = (ctx.debug_flag & RHIDebugFlag::FLAG_SHOW_UVS) != RHIDebugFlag::FLAG_NONE;
    bool runtime_affine_override = (ctx.debug_flag & RHIDebugFlag::FLAG_AFFINE_INTERPOLATION) != RHIDebugFlag::FLAG_NONE;
    

    // Render pipeline
    auto mesh_view = registry.view<tf::TFWorldTransformComponent, rrl::asset::MeshLinkage>();
    auto ui_view = registry.view<rrl::asset::TextureLinkage>();
    auto cam_view = registry.view<camera::CameraComponent, camera::CameraRuntimeComponent, camera::CameraOutputComponent>();

    cam_view.use<camera::CameraComponent>(); // Use CameraComponent as iteration drive to used the sorted view.
    
    // For each camera
    for (auto cam_entity : cam_view) {
        const auto& cam = cam_view.get<camera::CameraComponent>(cam_entity);
        const auto& cam_rt = cam_view.get<camera::CameraRuntimeComponent>(cam_entity);
        const auto& cam_out = cam_view.get<camera::CameraOutputComponent>(cam_entity);

        auto target_it = ctx.render_targets.find(cam_out.target_fbo);
        if (target_it == ctx.render_targets.end()) continue;

        // Render target textures
        SWRenderTarget& physical_rt = target_it->second;


        // Grab layout 0 (Color) as the primary target for UI and Backgrounds
        const SWRenderAttachment& primary_attach = physical_rt.color_attachments[0];
        if (primary_attach.handle == rhi::BACKEND_TEXTURE_NULL) continue;

        rrl::asset::ImageAsset& render_target = ctx.textures[primary_attach.handle];
        const auto& rt_format                 = ctx.tex_formats[primary_attach.handle];
        
        float half_w = static_cast<float>(physical_rt.width) * 0.5f;
        float half_h = static_cast<float>(physical_rt.height) * 0.5f;



        // --- Background Pass ---------------------------------------------
        // Handle Color/Background Mode
        bool needs_solid_clear = false;
        glm::vec4 clear_color = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f); // Fallback color
        if (cam.bg_mode == camera::CameraBackgroundMode::OVERRIDE_SOLID_COLOR) {
            needs_solid_clear = true;
            clear_color = cam.bg_override_clear_color;
        } 
        else if (cam.bg_mode == camera::CameraBackgroundMode::DEFAULT_SCENE_ENVIRONMENT) {
            if (ctx.environment_desc.type == rrl::scene::SceneEnvironmentType::SOLID_COLOR) {
                clear_color = ctx.environment_desc.clear_color;
            }
            needs_solid_clear = true; 
            // NOTE: SKYBOXES / HDRI MAPS ARE NOT IMPLEMENTED ON SOFTWARE RENDERING!!!
        }
        else if (cam.bg_mode == camera::CameraBackgroundMode::OVERRIDE_FLAT_TEXTURE) {
            if (cam_rt.resolved_bg_texture != BACKEND_TEXTURE_NULL && ctx.textures.find(cam_rt.resolved_bg_texture) != ctx.textures.end()) {
                const rrl::asset::ImageAsset& bg_tex = ctx.textures[cam_rt.resolved_bg_texture];
                const auto& bg_fmt = ctx.tex_formats[cam_rt.resolved_bg_texture];
                
                // Draw Fullscreen Texture
                software::SWRDrawTexture2D(
                    render_target, bg_tex, 
                    0, 0, render_target.width, render_target.height, 
                    rt_format, bg_fmt
                );
            } 
            // Missing texture fallback
            else {
                needs_solid_clear = true;
                clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); 
            }
        }

        // CPU memory fill for solid color clear
        if (needs_solid_clear) {
            uint8_t r = static_cast<uint8_t>(glm::clamp(clear_color.r, 0.0f, 1.0f) * 255.0f);
            uint8_t g = static_cast<uint8_t>(glm::clamp(clear_color.g, 0.0f, 1.0f) * 255.0f);
            uint8_t b = static_cast<uint8_t>(glm::clamp(clear_color.b, 0.0f, 1.0f) * 255.0f);
            
            if (render_target.color_layout == rrl::asset::ImageColorLayout::BGR) std::swap(r, b);
            
            size_t channels = (render_target.channels == rrl::asset::ImageChannelLayout::CH_4) ? 4 : 3;
            for (size_t i = 0; i < render_target.data.size(); i += channels) {
                render_target.data[i]   = r;
                render_target.data[i+1] = g;
                render_target.data[i+2] = b;
                if (channels == 4) render_target.data[i+3] = 255;
            }
        }


        // --- 3D Scene Pass -----------------------------------------------
        // For each mesh
        for (auto physical_entity : mesh_view) {
            const auto& world_tf = mesh_view.get<tf::TFWorldTransformComponent>(physical_entity);
            const auto& linkage = mesh_view.get<rrl::asset::MeshLinkage>(physical_entity);
            
            if (!registry.valid(linkage.mesh_asset)) continue;
            if (!registry.all_of<rrl::asset::MeshRuntimeComponent>(linkage.mesh_asset)) continue;
            if ((cam.culling_mask & linkage.layer_mask) == rhi::RHIRenderLayerMask::LAYER_NONE) continue;

            MeshHandle mesh_handle = registry.get<rrl::asset::MeshRuntimeComponent>(linkage.mesh_asset).handle;
            if (ctx.meshes.find(mesh_handle) == ctx.meshes.end()) continue;
            const auto& mesh = ctx.meshes[mesh_handle];

            glm::mat4 mvp = cam_rt.view_projection_matrix * world_tf.matrix;

            // Vertex Shader (vertex projection)
            software::SWRVertexShader(mesh, mvp, ctx.working_vertex_buffer);

            // Point Cloud Rasterization
            if (mesh.topology == rrl::asset::MeshTopology::POINTS) {
                for (size_t i = 0; i < mesh.active_vertex_count; ++i) {
                    const glm::vec4& ndc = ctx.working_vertex_buffer.ndc_positions[i];
                    
                    // Frustum Culled
                    if (ndc.w <= 0.0001f) continue; 
                    
                    // Viewport Transform
                    int px = static_cast<int>((ndc.x + 1.0f) * half_w);
                    int py = static_cast<int>((ndc.y + 1.0f) * half_h);
                    // int py = static_cast<int>((1.0f - ndc.y) * half_h); // Do not flip! OpenCV Convention!!
                    
                    software::SWRDrawPoint(
                        render_target, physical_rt.depth_buffer, 
                        glm::vec3(px, py, ndc.z), 2, 
                        glm::vec3(1.0f, 1.0f, 1.0f), 
                        rt_format
                    );
                }
                continue; 
            }
            
            // Triangles and Lines Rasterization
            else if (!mesh.indices.empty()) {
                // Fallback if the geometry has no submesh groups defined
                std::vector<rrl::asset::MeshSubmesh> default_submesh = {{0, static_cast<uint32_t>(mesh.indices.size())}};
                const std::vector<rrl::asset::MeshSubmesh>& active_submeshes = mesh.submeshes.empty() ? default_submesh : mesh.submeshes;

                for (size_t i = 0; i < active_submeshes.size(); ++i) {
                    const auto& submesh = active_submeshes[i];
                    
                    glm::vec4 mat_base_color(1.0f, 1.0f, 1.0f, 1.0f);
                    const rrl::asset::ImageAsset* active_albedo = nullptr;
                    software::ColorFormatCache tex_format{};
                    
                    // Fetch the material from the linkage component
                    entt::entity mat_entity = entt::null;
                    if (i < linkage.materials.size()) {
                        mat_entity = linkage.materials[i];
                    }
                    
                    // Resolve the material properties and textures
                    if (registry.valid(mat_entity) && registry.all_of<rrl::asset::MaterialRuntimeComponent>(mat_entity)) {
                        
                        const auto& mat_runtime = registry.get<rrl::asset::MaterialRuntimeComponent>(mat_entity);
                        
                        // Base material values
                        auto mat_it = ctx.materials.find(mat_runtime.handle);
                        if (mat_it != ctx.materials.end()) {
                            mat_base_color = mat_it->second.base_color;
                        }

                        // Material textures
                        if (mat_runtime.albedo_handle != rhi::BACKEND_TEXTURE_NULL) {
                            if (ctx.textures.find(mat_runtime.albedo_handle) != ctx.textures.end()) {
                                active_albedo = &ctx.textures[mat_runtime.albedo_handle];
                                tex_format = ctx.tex_formats[mat_runtime.albedo_handle];
                            }
                        }
                    }
                    software::SWRRender3DMesh(
                            render_target, physical_rt.depth_buffer, mesh, ctx.working_vertex_buffer,
                        submesh.index_offset, submesh.index_count, 
                        mat_base_color, active_albedo, 
                        rt_format, tex_format,
                        disable_textures, show_uvs, runtime_affine_override, draw_wireframes
                    );
                }
            }
        } // end mesh loop

        
        // --- 2D UI Pass --------------------------------------------------
        // For each UI obj
        for (auto ui_entity : ui_view) {
            const auto& linkage = ui_view.get<rrl::asset::TextureLinkage>(ui_entity);
            
            if (!registry.valid(linkage.texture_asset)) continue;
            if (!registry.all_of<rrl::asset::TextureRuntimeComponent>(linkage.texture_asset)) continue;
            
            // IMPORTANT: Camera visibility culling
            if ((linkage.layer_mask & cam.culling_mask) == rhi::RHIRenderLayerMask::LAYER_NONE) continue;
            if ((linkage.layer_mask & rhi::RHIRenderLayerMask::LAYER_UI) == rhi::RHIRenderLayerMask::LAYER_NONE) continue;
            
            rhi::TextureHandle tex_handle = registry.get<rrl::asset::TextureRuntimeComponent>(linkage.texture_asset).handle;
            if (ctx.textures.find(tex_handle) == ctx.textures.end()) continue;

            const rrl::asset::ImageAsset& source_tex = ctx.textures[tex_handle];
            const auto& src_fmt = ctx.tex_formats[tex_handle];
            
            int px = static_cast<int>(linkage.screen_x * render_target.width);
            int py = static_cast<int>(linkage.screen_y * render_target.height);
            int pw = static_cast<int>(linkage.screen_w * render_target.width);
            int ph = static_cast<int>(linkage.screen_h * render_target.height);
            
            software::SWRDrawTexture2D(
                render_target, source_tex, 
                px, py, pw, ph, 
                rt_format, src_fmt
            );
        } // end ui obj loop

    }

}



// --- Render Targets (FBOs) ---------------------------------------
static TextureHandle CreateRenderTexture(entt::registry& registry, uint32_t width, uint32_t height, 
                                         rrl::asset::ImageAssetType data_type,
                                         rrl::asset::ImageChannelLayout channels, 
                                         bool is_depth, uint32_t array_layers) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    TextureHandle handle = ctx.next_tex_handle++;
    
    rrl::asset::ImageAsset img;
    img.width = width;
    img.height = height;
    img.data_type = data_type;
    img.channels = channels;
    
    // Resolve channel count
    size_t channel_count = 1;
    if (channels == rrl::asset::ImageChannelLayout::CH_2) channel_count = 2;
    else if (channels == rrl::asset::ImageChannelLayout::CH_3) channel_count = 3;
    else if (channels == rrl::asset::ImageChannelLayout::CH_4) channel_count = 4;

    // Resolve bytes per channel
    size_t bytes_per_channel = 1;
    if (data_type == rrl::asset::ImageAssetType::UINT16) bytes_per_channel = 2;
    else if (data_type == rrl::asset::ImageAssetType::FLOAT32) bytes_per_channel = 4;

    img.color_layout = rrl::asset::ImageColorLayout::RGB;
    
    // Calculate total size including array layers
    size_t slice_bytes = width * height * channel_count * bytes_per_channel;
    size_t total_bytes = slice_bytes * std::max<uint32_t>(1, array_layers);
    
    img.data.resize(total_bytes, 0);
    
    ctx.textures[handle] = std::move(img);
    ctx.tex_formats[handle] = software::GetColorFormatCache(ctx.textures[handle].color_layout, channels);
    return handle;
}
static RenderTargetHandle CreateRenderTarget(entt::registry& registry, const PhysicalRenderTargetDescriptor& desc) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    
    RenderTargetHandle handle = ctx.next_handle++;
    
    SWRenderTarget rt;
    rt.width = desc.width;
    rt.height = desc.height;
    
    // Map attachments strictly by layout_location
    for (const auto& attach : desc.color_attachments) {
        if (attach.layout_location < MAX_COLOR_ATTACHMENTS) {
            
            // Calculate where this specific camera slice starts in the memory block
            size_t slice_byte_offset = 0;
            if (attach.is_texture_array && ctx.textures.find(attach.handle) != ctx.textures.end()) {
                const auto& img = ctx.textures[attach.handle];
                size_t bpp = (img.data_type == rrl::asset::ImageAssetType::FLOAT32) ? 4 : 
                             (img.data_type == rrl::asset::ImageAssetType::UINT16) ? 2 : 1;
                size_t channels = (img.channels == rrl::asset::ImageChannelLayout::CH_4) ? 4 : 
                                  (img.channels == rrl::asset::ImageChannelLayout::CH_3) ? 3 : 
                                  (img.channels == rrl::asset::ImageChannelLayout::CH_2) ? 2 : 1;
                
                size_t slice_size = rt.width * rt.height * channels * bpp;
                slice_byte_offset = attach.array_idx * slice_size;
            }
            rt.color_attachments[attach.layout_location] = {
                attach.handle,
                attach.array_idx,
                slice_byte_offset
            };
        }
    }
    
    // Allocate internal hardware Z-buffer for this target
    rt.depth_buffer.resize(rt.width * rt.height, std::numeric_limits<float>::max());
    ctx.render_targets[handle] = std::move(rt);
    return handle;
}
static void DestroyRenderTarget(entt::registry& registry, RenderTargetHandle handle) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    if (handle == rhi::BACKEND_TARGET_MAIN) return;
    
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.render_targets.erase(handle);
}



// --- Textures ----------------------------------------------------
static void UpdateTexture(entt::registry& registry, TextureHandle handle, const rrl::asset::ImageAsset& image_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    RRL_ASSERT(image_data.IsValid(), "SoftwareBackend received an invalid image model for texture updating!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    if (ctx.textures.find(handle) == ctx.textures.end() || image_data.data.empty()) return;

    ctx.textures[handle] = image_data;
    ctx.tex_formats[handle] = software::GetColorFormatCache(image_data.color_layout, image_data.channels);
}
static TextureHandle CreateTexture(entt::registry& registry, const rrl::asset::ImageAsset& image_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    RRL_ASSERT(image_data.IsValid(), "OpenCVRenderer received an invalid image model for texture creation!");
    auto& ctx = registry.ctx().get<SoftwareContext>();

    TextureHandle handle = ctx.next_tex_handle++;
    ctx.textures[handle] = rrl::asset::ImageAsset{};
    UpdateTexture(registry, handle, image_data);
    return handle;
}
static void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.textures.erase(handle);
    ctx.tex_formats.erase(handle);
}



// --- Meshes ------------------------------------------------------
static void UpdateMesh(entt::registry& registry, MeshHandle handle, const rrl::asset::MeshAsset& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    if (ctx.meshes.find(handle) != ctx.meshes.end()) {
        software::LoadMeshAssetIntoSWRMesh(mesh_data, ctx.meshes[handle]);
    }
}
static MeshHandle CreateMesh(entt::registry& registry, const rrl::asset::MeshAsset& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    MeshHandle handle = ctx.next_mesh_handle++;
    
    software::LoadMeshAssetIntoSWRMesh(mesh_data, ctx.meshes[handle]);
    return handle;
}
static void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.meshes.erase(handle);
}



// --- Materials ---------------------------------------------------
static void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const rrl::asset::MaterialAsset& material_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    if (ctx.materials.find(handle) != ctx.materials.end()) {
        ctx.materials[handle] = material_data;
    }
}
static MaterialHandle CreateMaterial(entt::registry& registry, const rrl::asset::MaterialAsset& material_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    MaterialHandle handle = ctx.next_mat_handle++;
    
    ctx.materials[handle] = material_data;
    return handle;
}
static void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.materials.erase(handle);
}



// --- Environment -------------------------------------------------
static void SetEnvironment(entt::registry& registry, const PhysicalEnvironmentDescriptor& env) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.environment_desc = env;
}



// --- Presentation ------------------------------------------------
static rrl::asset::ImageAsset GetTargetImage(entt::registry& registry, RenderTargetHandle handle, rhi::RHIRenderAttachmentSemantic semantic, uint32_t array_index) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();

    if (ctx.render_targets.find(handle) == ctx.render_targets.end()) return rrl::asset::ImageAsset{};
    const SWRenderTarget& rt = ctx.render_targets[handle];
    
    // Explicitly resolve the layout location using the mapping function
    size_t layout_loc = MapSemanticToLayoutLocation(semantic);
    
    if (layout_loc >= MAX_COLOR_ATTACHMENTS) return {};
    
    // Map Semantic layout location to the FBO attachment
    SWRenderAttachment attach = rt.color_attachments[layout_loc];
    
    // Fallback: grab the first available valid color attachment.
    if (attach.handle == rhi::BACKEND_TEXTURE_NULL) {
        for (const auto& a : rt.color_attachments) {
            if (a.handle != rhi::BACKEND_TEXTURE_NULL) {
                attach = a;
                break;
            }
        }
    }

    // This FBO has no color attachments at all (Depth only)
    if (attach.handle == rhi::BACKEND_TEXTURE_NULL) return {};
    if (ctx.textures.find(attach.handle) == ctx.textures.end()) return {};

    const rrl::asset::ImageAsset& source_array = ctx.textures[attach.handle];

    // Calculate memory slice size
    size_t channels = (source_array.channels == rrl::asset::ImageChannelLayout::CH_4) ? 4 : 
                      (source_array.channels == rrl::asset::ImageChannelLayout::CH_3) ? 3 : 
                      (source_array.channels == rrl::asset::ImageChannelLayout::CH_2) ? 2 : 1;
    
    size_t bpp = (source_array.data_type == rrl::asset::ImageAssetType::FLOAT32) ? 4 : 
                 (source_array.data_type == rrl::asset::ImageAssetType::UINT16) ? 2 : 1;
                 
    size_t slice_bytes = source_array.width * source_array.height * channels * bpp;
    size_t read_offset = array_index * slice_bytes;

    // Bounds check
    if (read_offset + slice_bytes > source_array.data.size()) {
        LOG_WARN("[Software RHI] Requested array_index {} out of bounds for texture. Clamping to 0.", array_index);
        read_offset = 0;
        if (source_array.data.size() < slice_bytes) return {}; // Safety bailout
    }

    // Copy the requested slice into a new ImageAsset
    rrl::asset::ImageAsset slice_img;
    slice_img.width = source_array.width;
    slice_img.height = source_array.height;
    slice_img.data_type = source_array.data_type;
    slice_img.channels = source_array.channels;
    slice_img.color_layout = source_array.color_layout;
    slice_img.origin = source_array.origin;
    slice_img.filter = source_array.filter;
    
    slice_img.data.resize(slice_bytes);
    std::memcpy(slice_img.data.data(), source_array.data.data() + read_offset, slice_bytes);

    return slice_img;
}
static void Present(entt::registry& registry, RenderTargetHandle handle, rhi::RHIRenderAttachmentSemantic semantic, uint32_t array_index) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    const RHIWindow* window = ctx.active_window;

    if (window == nullptr || window->type == RHIWindowType::HEADLESS) return;
    
    // Grab the specific texture slice
    rrl::asset::ImageAsset raw_img = GetTargetImage(registry, handle, semantic, array_index);
    if (raw_img.data.empty()) return;

    // OpenCV Window Presentation Support
    if (window->type == RHIWindowType::OPENCV) {
        #ifdef RRL_BUILD_WINDOW_OPENCV 
        const char* win_name = static_cast<const char*>(window->native_handle);

        // Wrap the raw buffer
        int cv_type = CV_8UC3;
        if (raw_img.data_type == rrl::asset::ImageAssetType::UINT8) {
            cv_type = (raw_img.channels == rrl::asset::ImageChannelLayout::CH_4) ? CV_8UC4 : (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1) ? CV_8UC1 : CV_8UC3;
        } else if (raw_img.data_type == rrl::asset::ImageAssetType::FLOAT32) {
            cv_type = (raw_img.channels == rrl::asset::ImageChannelLayout::CH_4) ? CV_32FC4 : (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1) ? CV_32FC1 : CV_32FC3;
        } else if (raw_img.data_type == rrl::asset::ImageAssetType::UINT16) {
            cv_type = (raw_img.channels == rrl::asset::ImageChannelLayout::CH_4) ? CV_16UC4 : (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1) ? CV_16UC1 : CV_16UC3;
        }
        cv::Mat fbo_mat(raw_img.height, raw_img.width, cv_type, raw_img.data.data());

        // Coordinate origin flipping
        cv::Mat ready_mat;
        if (raw_img.origin == rrl::asset::ImageOrigin::BOTTOM_LEFT) {
            cv::flip(fbo_mat, ready_mat, 0); // 0 = flip vertically
        } else {
            ready_mat = fbo_mat;
        }

        // Color Space Conversion
        cv::Mat final_mat;
        if (raw_img.color_layout == rrl::asset::ImageColorLayout::RGB) {
            cv::cvtColor(ready_mat, final_mat, cv::COLOR_RGB2BGR);
        }
        else if (raw_img.color_layout == rrl::asset::ImageColorLayout::RGBA) {
            cv::cvtColor(ready_mat, final_mat, cv::COLOR_RGBA2BGRA);
        }
        else {
            final_mat = ready_mat;
        }
        
        // Normalize float images
        if (raw_img.data_type == rrl::asset::ImageAssetType::FLOAT32) {
            cv::normalize(final_mat, final_mat, 0.0, 1.0, cv::NORM_MINMAX);
        }

        // Scale to viewport and Present
        if (ctx.active_window->width != ctx.render_width || ctx.active_window->height != ctx.render_height) {
            cv::Mat scaled_mat;
            cv::resize(final_mat, scaled_mat, cv::Size(ctx.active_window->width, ctx.active_window->height), 0, 0, cv::INTER_NEAREST);
            cv::imshow(win_name, scaled_mat);
        } else {
            cv::imshow(win_name, final_mat);
        }
        #else

        LOG_ERROR("SoftwareBackend was requested to present the rendered FBO on an OpenCV window but OpenCV support is not compiled on the library");
        #endif
        return;
    }

    // GLFW Window Presentation Support
    else if (ctx.active_window->type == RHIWindowType::GLFW) {
        #ifdef RRL_BUILD_WINDOW_GLFW
        GLFWwindow* gl_window = static_cast<GLFWwindow*>(window->native_handle);
        if (!gl_window) return;

        glfwMakeContextCurrent(gl_window);
        if (!ctx.gl_pointers_loaded) {
            gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
            ctx.gl_pointers_loaded = true;
        }
        if (ctx.presentation_tex == 0) {
            glGenTextures(1, &ctx.presentation_tex);
            glGenFramebuffers(1, &ctx.presentation_fbo);
        }
        glBindTexture(GL_TEXTURE_2D, ctx.presentation_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // OpenGL Format Mapping
        GLenum gl_internal_format   = GL_RGB8;
        GLenum gl_type              = GL_UNSIGNED_BYTE;
        if (raw_img.data_type == rrl::asset::ImageAssetType::UINT8) {
            gl_type = GL_UNSIGNED_BYTE;
            if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1)        gl_internal_format = GL_R8;
            else if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_3)   gl_internal_format = GL_RGB8;
            else if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_4)   gl_internal_format = GL_RGBA8;
        } 
        else if (raw_img.data_type == rrl::asset::ImageAssetType::FLOAT32) {
            gl_type = GL_FLOAT;
            if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1)        gl_internal_format = GL_R32F;
            else if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_3)   gl_internal_format = GL_RGB32F;
            else if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_4)   gl_internal_format = GL_RGBA32F;
        }

        // OpenGL Color Layout Mapping
        GLenum gl_format = GL_RGB;
        if (raw_img.color_layout == rrl::asset::ImageColorLayout::RGB)       gl_format = GL_RGB;
        else if (raw_img.color_layout == rrl::asset::ImageColorLayout::BGR)  gl_format = GL_BGR;
        else if (raw_img.color_layout == rrl::asset::ImageColorLayout::RGBA) gl_format = GL_RGBA;
        else if (raw_img.color_layout == rrl::asset::ImageColorLayout::BGRA) gl_format = GL_BGRA;
        else if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1)   gl_format = GL_RED;

        // OpenGL Image Origin Mapping
        // If the CPU image is Top-Left, we must flip it vertically for OpenGL's Bottom-Left screen.
        int win_w, win_h;
        int dst_y0 = 0;
        int dst_y1 = win_h;
        glfwGetFramebufferSize(gl_window, &win_w, &win_h);
        if (raw_img.origin == rrl::asset::ImageOrigin::TOP_LEFT) {
            dst_y0 = win_h;
            dst_y1 = 0;
        }

        // Upload CPU pixels to GPU
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (ctx.last_blit_w != raw_img.width || ctx.last_blit_h != raw_img.height) {
            glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, raw_img.width, raw_img.height, 0, gl_format, gl_type, raw_img.data.data());
            ctx.last_blit_w = raw_img.width;
            ctx.last_blit_h = raw_img.height;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, raw_img.width, raw_img.height, gl_format, gl_type, raw_img.data.data());
        }

        // Hardware Presentation (Blitting)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.presentation_fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.presentation_tex, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind native OS window
        glBlitFramebuffer(
            0, 0, raw_img.width, raw_img.height,  // Source Rect
            0, dst_y0, win_w, dst_y1,               // Dest Rect (Dynamically flipped)
            GL_COLOR_BUFFER_BIT, 
            GL_NEAREST
        );
        glfwSwapBuffers(gl_window);

        #else
        LOG_ERROR("SoftwareBackend was requested to present on a GLFW window but GLFW support is not compiled.");
        #endif
        return;
    }

}
static void OnWindowDestroyed(entt::registry& registry, const RHIWindow* window) {
    if (!registry.ctx().contains<SoftwareContext>()) return;
    auto& ctx = registry.ctx().get<SoftwareContext>();

    // If the window being deleted is our active render canvas, break the link!
    if (ctx.active_window == window) {
        LOG_WARN("[Software RHI] Active presentation window destroyed by application. Detaching safely to prevent segfaults.");
        ctx.active_window = nullptr;
    }
}


// --- Debugging ---------------------------------------------------
static void SetDebugFlag(entt::registry& registry, RHIDebugFlag flag, bool enable) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();

    if (enable) ctx.debug_flag |= flag;
    else ctx.debug_flag &= ~flag;
}
static RHIDebugFlag GetActiveDebugFlags(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    return registry.ctx().get<SoftwareContext>().debug_flag;
}


// --- Creation ----------------------------------------------------
RHIBackend CreateSoftwareBackend() {
    RHIBackend backend;
    backend.type                = RHIBackendType::SOFTWARE;

    // Lifecycle
    backend.Initialize          = Initialize;
    backend.Shutdown            = Shutdown;
    backend.RenderFrame         = RenderFrame;

    // Target FBOs
    backend.CreateRenderTexture = CreateRenderTexture;
    backend.CreateRenderTarget  = CreateRenderTarget;
    backend.DestroyRenderTarget = DestroyRenderTarget;

    // Textures
    backend.CreateTexture       = CreateTexture;
    backend.UpdateTexture       = UpdateTexture;
    backend.DestroyTexture      = DestroyTexture;

    // Meshes
    backend.CreateMesh          = CreateMesh;
    backend.UpdateMesh          = UpdateMesh;
    backend.DestroyMesh         = DestroyMesh;

    // Materials
    backend.CreateMaterial      = CreateMaterial;
    backend.UpdateMaterial      = UpdateMaterial;
    backend.DestroyMaterial     = DestroyMaterial;

    // Environment
    backend.SetEnvironment      = SetEnvironment;

    // Presentation
    backend.GetTargetImage      = GetTargetImage;
    backend.Present             = Present;
    backend.OnWindowDestroyed   = OnWindowDestroyed;
    
    // Debugging
    backend.SetDebugFlag        = SetDebugFlag;
    backend.GetActiveDebugFlags = GetActiveDebugFlags;
    
    return backend;
}


} // namespace rrl::rhi::software
