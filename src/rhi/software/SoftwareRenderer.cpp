// RRL/src/rhi/software/SoftwareRenderer.cpp


// Runtime components
#include "RRL/camera/CameraComponents.hpp"
#include "RRL/tf/TFComponents.hpp"
#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MaterialComponents.hpp"


#include "RRL/data/ImageData.hpp"
#include "RRL/rhi/RHILayers.hpp"
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


/**
 * @brief Holds the running Software rendering backend context.
 */
struct SoftwareContext {
    uint32_t render_width;
    uint32_t render_height;
    const RHIWindow* active_window { nullptr };
    
    // Core Engine Data Buffers 
    std::unordered_map<RenderTargetHandle, data::ImageData> render_targets;
    std::unordered_map<RenderTargetHandle, data::ImageData> depth_buffers;
    std::unordered_map<RenderTargetHandle, software::ColorFormatCache> rt_formats;

    std::unordered_map<TextureHandle, data::ImageData> textures;
    std::unordered_map<TextureHandle, software::ColorFormatCache> tex_formats;

    // Assets
    std::unordered_map<MeshHandle, software::SWRMesh> meshes; 
    std::unordered_map<MaterialHandle, data::MaterialData> materials;

    // Vertex Shader Working Buffer 
    software::SWRVertexBuffer working_vertex_buffer;

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

    // Create TARGET_MAIN
    data::ImageData rt;
    rt.width = render_width; 
    rt.height = render_height; 
    rt.data_type = data::ImageDataType::UINT8;
    rt.channels = data::ImageChannelLayout::CH_3;
    rt.color_layout = data::ImageColorLayout::BGR;
    rt.data.resize(render_width * render_height * 3, 30);
    ctx.render_targets[TARGET_MAIN] = std::move(rt);
    ctx.rt_formats[TARGET_MAIN]     = software::GetColorFormatCache(rt.color_layout, rt.channels);
    RRL_ASSERT(rt.IsImageModelValid(), "[Software RHI] Error creating TARGET_MAIN image");
    
    // Create Depth buffer
    data::ImageData depth;
    depth.width = render_width; 
    depth.height = render_height;
    depth.data_type = data::ImageDataType::FLOAT32;
    depth.channels = data::ImageChannelLayout::CH_1; 
    depth.color_layout = data::ImageColorLayout::NONE; 
    depth.data.resize(render_width * render_height * sizeof(float));
    ctx.depth_buffers[TARGET_MAIN] = std::move(depth);
    RRL_ASSERT(depth.IsImageModelValid(), "[Software RHI] Error creating TARGET_MAIN depth buffer");

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

    // Fast-clear all active targets in RAM
    for (auto& [handle, target] : ctx.render_targets) {
        std::fill(target.data.begin(), target.data.end(), 30);
        
        auto& depth = ctx.depth_buffers[handle];
        float* d_ptr = reinterpret_cast<float*>(depth.data.data());
        std::fill(d_ptr, d_ptr + (depth.width * depth.height), std::numeric_limits<float>::max());
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
        const auto& rt_format = ctx.rt_formats[cam.target_fbo];

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
                    
                    software::SWRDrawPoint(
                        render_target, depth_buffer, 
                        glm::vec3(px, py, ndc.z), 2, 
                        glm::vec3(1.0f, 1.0f, 1.0f), 
                        rt_format
                    );
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
                        // Material resolution
                        const auto& mat_runtime = registry.get<data::MaterialRuntimeComponent>(submesh.material_entity);
                        
                        // Base material values
                        auto mat_it = ctx.materials.find(mat_runtime.handle);
                        if (mat_it != ctx.materials.end()) {
                            mat_base_color = mat_it->second.base_color;
                        }

                        // Material textures
                        if (mat_runtime.albedo_handle != rhi::TEXTURE_NULL) {
                            if (ctx.textures.find(mat_runtime.albedo_handle) != ctx.textures.end()) {
                                active_albedo = &ctx.textures[mat_runtime.albedo_handle];
                                tex_format = ctx.tex_formats[mat_runtime.albedo_handle];
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
    data::ImageData& main_target = ctx.render_targets[TARGET_MAIN];
    const auto& main_fmt = ctx.rt_formats[TARGET_MAIN];
    for (auto ui_entity : ui_view) {
        const auto& linkage = ui_view.get<data::TextureLinkage>(ui_entity);
        
        if (!registry.valid(linkage.texture_asset)) continue;
        if (!registry.all_of<data::TextureRuntimeComponent>(linkage.texture_asset)) continue;
        if ((linkage.layer_mask & rhi::RenderLayer::LAYER_UI) == rhi::RenderLayer::LAYER_NONE) continue;
        
        rhi::TextureHandle tex_handle = registry.get<data::TextureRuntimeComponent>(linkage.texture_asset).handle;
        if (ctx.textures.find(tex_handle) == ctx.textures.end()) continue;

        const data::ImageData& source_tex = ctx.textures[tex_handle];
        const auto& src_fmt = ctx.tex_formats[tex_handle];
        
        int px = static_cast<int>(linkage.screen_x * main_target.width);
        int py = static_cast<int>(linkage.screen_y * main_target.height);
        int pw = static_cast<int>(linkage.screen_w * main_target.width);
        int ph = static_cast<int>(linkage.screen_h * main_target.height);
        
        // Utilize the new primitive drawing API for 2D UI Blitting
        software::SWRDrawTexture2D(
            main_target, source_tex, 
            px, py, pw, ph, 
            main_fmt, src_fmt
        );
    }
}



// --- Render Targets (FBOs) ---------------------------------------
static RenderTargetHandle CreateTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    
    RenderTargetHandle handle = ctx.next_handle++;
    
    data::ImageData rt;
    rt.width = width; rt.height = height; 
    rt.data_type = data::ImageDataType::UINT8;
    rt.channels = data::ImageChannelLayout::CH_3;
    rt.color_layout = data::ImageColorLayout::RGB;
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

    return handle;
}
static void DestroyTarget(entt::registry& registry, RenderTargetHandle handle) {
RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    if (handle == TARGET_MAIN) return;
    
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.render_targets.erase(handle);
    ctx.depth_buffers.erase(handle);
    ctx.rt_formats.erase(handle);
}



// --- Textures ----------------------------------------------------
static void UpdateTexture(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    RRL_ASSERT(image_data.IsImageModelValid(), "SoftwareBackend received an invalid image model for texture updating!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    if (ctx.textures.find(handle) == ctx.textures.end() || image_data.data.empty()) return;

    ctx.textures[handle] = image_data;
    ctx.tex_formats[handle] = software::GetColorFormatCache(image_data.color_layout, image_data.channels);
}
static TextureHandle CreateTexture(entt::registry& registry, const data::ImageData& image_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    RRL_ASSERT(image_data.IsImageModelValid(), "OpenCVRenderer received an invalid image model for texture creation!");
    auto& ctx = registry.ctx().get<SoftwareContext>();

    TextureHandle handle = ctx.next_tex_handle++;
    ctx.textures[handle] = data::ImageData{};
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
static void UpdateMesh(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    if (ctx.meshes.find(handle) != ctx.meshes.end()) {
        software::LoadMeshDataIntoSWRMesh(mesh_data, ctx.meshes[handle]);
    }
}
static MeshHandle CreateMesh(entt::registry& registry, const data::MeshData& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    MeshHandle handle = ctx.next_mesh_handle++;
    
    software::LoadMeshDataIntoSWRMesh(mesh_data, ctx.meshes[handle]);
    return handle;
}
static void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    ctx.meshes.erase(handle);
}



// --- Materials ---------------------------------------------------
static void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    if (ctx.materials.find(handle) != ctx.materials.end()) {
        ctx.materials[handle] = material_data;
    }
}
static MaterialHandle CreateMaterial(entt::registry& registry, const data::MaterialData& material_data) {
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


// --- Presentation ------------------------------------------------
static data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();

    // Return a copy of the raw ImageData 
    if (ctx.render_targets.find(handle) == ctx.render_targets.end()) return data::ImageData{};
    return ctx.render_targets[handle];
}
static void Present(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SoftwareContext>(), "SoftwareBackend not initialized!");
    auto& ctx = registry.ctx().get<SoftwareContext>();
    const RHIWindow* window = ctx.active_window;

    if (window == nullptr || window->type == RHIWindowType::HEADLESS) return;

    // OpenCV Window Presentation Support
    if (window->type == RHIWindowType::OPENCV) {
        #ifdef RRL_BUILD_WINDOW_OPENCV 
        const char* win_name = static_cast<const char*>(window->native_handle);
        data::ImageData& main_fbo = ctx.render_targets[TARGET_MAIN];
        cv::Mat fbo_mat(main_fbo.height, main_fbo.width, CV_8UC3, main_fbo.data.data());
        
        // Scale to the Window Size if it differs from the rendering resolution
        if (window->width != ctx.render_width || window->height != ctx.render_height) {
            cv::Mat scaled_fbo;
            cv::resize(fbo_mat, scaled_fbo, cv::Size(window->width, window->height), 0, 0, cv::INTER_NEAREST);
            cv::imshow(win_name, scaled_fbo);
        } else {
            cv::imshow(win_name, fbo_mat);
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

        // Fetch data
        data::ImageData main_fbo = GetTargetImage(registry, TARGET_MAIN);
        if (main_fbo.data.empty()) return;

        // OpenGL Format Mapping
        GLenum gl_internal_format   = GL_RGB8;
        GLenum gl_type              = GL_UNSIGNED_BYTE;
        if (main_fbo.data_type == data::ImageDataType::UINT8) {
            gl_type = GL_UNSIGNED_BYTE;
            if (main_fbo.channels == data::ImageChannelLayout::CH_1)        gl_internal_format = GL_R8;
            else if (main_fbo.channels == data::ImageChannelLayout::CH_3)   gl_internal_format = GL_RGB8;
            else if (main_fbo.channels == data::ImageChannelLayout::CH_4)   gl_internal_format = GL_RGBA8;
        } 
        else if (main_fbo.data_type == data::ImageDataType::FLOAT32) {
            gl_type = GL_FLOAT;
            if (main_fbo.channels == data::ImageChannelLayout::CH_1)        gl_internal_format = GL_R32F;
            else if (main_fbo.channels == data::ImageChannelLayout::CH_3)   gl_internal_format = GL_RGB32F;
            else if (main_fbo.channels == data::ImageChannelLayout::CH_4)   gl_internal_format = GL_RGBA32F;
        }

        // OpenGL Color Layout Mapping
        GLenum gl_format = GL_RGB;
        if (main_fbo.color_layout == data::ImageColorLayout::RGB)       gl_format = GL_RGB;
        else if (main_fbo.color_layout == data::ImageColorLayout::BGR)  gl_format = GL_BGR;
        else if (main_fbo.color_layout == data::ImageColorLayout::RGBA) gl_format = GL_RGBA;
        else if (main_fbo.color_layout == data::ImageColorLayout::BGRA) gl_format = GL_BGRA;
        else if (main_fbo.channels == data::ImageChannelLayout::CH_1)   gl_format = GL_RED;

        // OpenGL Image Origin Mapping
        // If the CPU image is Top-Left, we must flip it vertically for OpenGL's Bottom-Left screen.
        int win_w, win_h;
        int dst_y0 = 0;
        int dst_y1 = win_h;
        glfwGetFramebufferSize(gl_window, &win_w, &win_h);
        if (main_fbo.origin == data::ImageOrigin::TOP_LEFT) {
            dst_y0 = win_h;
            dst_y1 = 0;
        }

        // Upload CPU pixels to GPU
        if (ctx.last_blit_w != main_fbo.width || ctx.last_blit_h != main_fbo.height) {
            glTexImage2D(GL_TEXTURE_2D, 0, gl_internal_format, main_fbo.width, main_fbo.height, 0, gl_format, gl_type, main_fbo.data.data());
            ctx.last_blit_w = main_fbo.width;
            ctx.last_blit_h = main_fbo.height;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, main_fbo.width, main_fbo.height, gl_format, gl_type, main_fbo.data.data());
        }

        // Hardware Presentation (Blitting)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx.presentation_fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.presentation_tex, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Bind native OS window
        glBlitFramebuffer(
            0, 0, main_fbo.width, main_fbo.height,  // Source Rect
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
    backend.CreateRenderTarget  = CreateTarget;
    backend.DestroyRenderTarget = DestroyTarget;

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
