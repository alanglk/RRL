// RRL/src/rhi/software/OpenGLRenderer.cpp


// Runtime components
#include "RRL/camera/CameraComponents.hpp"
#include "RRL/asset/MeshAsset.hpp"
#include "RRL/tf/TFComponents.hpp"
#include "RRL/asset/TextureComponents.hpp"
#include "RRL/asset/MeshComponents.hpp"
#include "RRL/asset/MaterialComponents.hpp"


#include "RRL/asset/ImageAsset.hpp"
#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/rhi/opengl/ShaderManager.hpp"

#include <entt/entt.hpp>
#include <glad/glad.h>

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


// OpenCV Window Presentation Support
#ifdef RRL_BUILD_WINDOW_OPENCV 
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#endif


// GLFW Window Presentation Support (default)
#include <GLFW/glfw3.h>



namespace rrl::rhi::opengl {


/**
 * @brief Holds OpenGL render target GL ids.
 */
struct GLRenderTarget {
    GLuint fbo = 0;
    GLuint color_texture = 0;
    GLuint depth_texture = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

/**
 * @brief Holds OpenGL loaded texture ids;
 */
struct GLTexture {
    GLuint id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    GLenum upload_format = 0; 
    GLenum upload_type = 0;
};

/**
 * @brief Holds OpenGL mesh VAO / VBOs data.
 */
struct GLMesh {
    GLuint vao = 0;
    GLuint vbo_positions = 0;
    GLuint vbo_normals = 0;
    GLuint vbo_uvs = 0;
    GLuint vbo_colors = 0;
    GLuint ebo = 0;
    
    uint32_t vertex_count = 0; // Active vertices to draw
    uint32_t index_count = 0;  // Active indices to draw

    // VRAM allocation tracking
    uint32_t vertex_capacity = 0;
    uint32_t index_capacity = 0;

    std::vector<rrl::asset::MeshSubmesh> submeshes;
    GLenum topology = GL_TRIANGLES;
};

/**
 * @brief Structure for sending UBO Material data to the GPU.
 * Aligned C++ struct matching the GLSL std140 layout.
 */
struct MaterialUBOData {
    alignas(16) glm::vec4 base_color;
    alignas(16) glm::vec3 emission;
    alignas(4)  float roughness;
    alignas(4)  float metallic;
    alignas(4)  float padding[3]; // Pad to a multiple of 16 bytes
};

/**
 * @brief Holds OpenGL material UBOs and resolved texture IDs.
 */
struct GLMaterial {
    GLuint ubo = 0;
};



/**
 * @brief Holds the running OpenGL hardware rendering backend context.
 */
struct OpenGLContext {
    uint32_t render_width = 0;
    uint32_t render_height = 0;
    const RHIWindow* active_window { nullptr };

    // Core Engine Data Buffers 
    std::unordered_map<RenderTargetHandle, GLRenderTarget>  render_targets;
    std::unordered_map<TextureHandle, GLTexture>            textures;
    std::unordered_map<MeshHandle, GLMesh>                  meshes;
    std::unordered_map<MaterialHandle, GLMaterial>          materials;

    // Next handle counters
    RenderTargetHandle next_target_handle = 1;
    TextureHandle next_tex_handle = 1;
    MeshHandle next_mesh_handle = 1;
    MaterialHandle next_mat_handle = 1;
    
    // UI VAO to render a full quad texture
    GLuint ui_empty_vao = 0;

    // Offscreen fallback context for HEADLESS and OPENCV modes
    GLFWwindow* hidden_window { nullptr };
};



// Helper to map RRL topology to OpenGL
static GLenum MapTopology(rrl::asset::MeshTopology topology) {
    switch (topology) {
        case rrl::asset::MeshTopology::POINTS: return GL_POINTS;
        case rrl::asset::MeshTopology::LINES: return GL_LINES;
        case rrl::asset::MeshTopology::TRIANGLES: return GL_TRIANGLES;
        default: return GL_TRIANGLES;
    }
}


// Fordward declarations
static RenderTargetHandle CreateRenderTarget(entt::registry& registry, uint32_t width, uint32_t height); 
static void DestroyRenderTarget(entt::registry& registry, RenderTargetHandle handle);
static void DestroyTexture(entt::registry& registry, TextureHandle handle);
static void DestroyMaterial(entt::registry& registry, MaterialHandle handle);
static void DestroyMesh(entt::registry& registry, MeshHandle handle);

// --- Lifecycle ---------------------------------------------------
static bool Initialize(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window) {
    RRL_ASSERT(window != nullptr, "[OpenGL RHI] Received a null window ptr");
    if (!glfwInit()) {
        LOG_ERROR("[OpenGL RHI] Failed to initialize GLFW.");
        return false;
    }

    auto& ctx = registry.ctx().emplace<OpenGLContext>();
    ctx.render_width = render_width;
    ctx.render_height = render_height;
    ctx.active_window = window;

    GLFWwindow* target_gl_context = nullptr;

    // Headless and OpenCV offscreen setups
    if (window->type == RHIWindowType::HEADLESS || window->type == RHIWindowType::OPENCV) {
        LOG_WARN("[OpenGL RHI] Headless or OpenCV window detected. Creating a hidden offscreen GLFW context.");
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Headless execution

        ctx.hidden_window = glfwCreateWindow(render_width, render_height, "RRL OpenGL Offscreen", nullptr, nullptr);
        if (!ctx.hidden_window) {
            LOG_ERROR("[OpenGL RHI] Failed to create hidden GLFW offscreen context.");
            registry.ctx().erase<OpenGLContext>();
            return false;
        }
        target_gl_context = ctx.hidden_window;
    } 
    else if (window->type == RHIWindowType::GLFW) {
        RRL_ASSERT(window->native_handle != nullptr, "[OpenGL RHI] GLFW Window is missing its native handle!");
        target_gl_context = static_cast<GLFWwindow*>(window->native_handle);
    }

    // Bind context to the executing thread and load GL function pointers
    static bool glad_initialized = false;
    glfwMakeContextCurrent(target_gl_context);
    if (!glad_initialized) {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            LOG_ERROR("[OpenGL RHI] GLAD failed to load OpenGL 4.5 Core function pointers.");
            if (ctx.hidden_window) {
                glfwDestroyWindow(ctx.hidden_window);
            }
            registry.ctx().erase<OpenGLContext>();
            return false;
        }
        glad_initialized = true;
    }

    // Configure global OpenGL configurations default states
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // Clockwise triangles are the front (left-handed coord system)
    glFrontFace(GL_CW);

    // attribute location 3 -> Color. If missing default to white
    glVertexAttrib4f(3, 1.0f, 1.0f, 1.0f, 1.0f);
    
    // Set up default viewport configuration
    glViewport(0, 0, render_width, render_height);
    

    // Initialize TARGET_MAIN
    ctx.next_target_handle = TARGET_MAIN; // Which equals 0
    CreateRenderTarget(registry, render_width, render_height);
    glCreateVertexArrays(1, &ctx.ui_empty_vao);

    // Load shaders
    #ifdef RRL_EMBED_SHADERS
    ShaderManager::Instance().LoadShadersFromSource();
    #else
    // Using stringification to convert the CMake macro to a standard string
    #define STR(x) #x
    #define XSTR(x) STR(x)
    std::string shader_dir = XSTR(RRL_OPENGL_RUNTIME_SHADER_DIR);
    shader_dir.erase(std::remove(shader_dir.begin(), shader_dir.end(), '\"'), shader_dir.end());
    ShaderManager::I().LoadShadersFromDirectory(shader_dir);
    #endif

    
    LOG_INFO("[OpenGL RHI] Initialized successfully on GPU: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    return true;
}
static void Shutdown(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();

    GLFWwindow* target_context = nullptr;
    if (ctx.hidden_window) {
        target_context = ctx.hidden_window;
    } 
    else if (ctx.active_window && ctx.active_window->type == RHIWindowType::GLFW) {
        target_context = static_cast<GLFWwindow*>(ctx.active_window->native_handle);
    }
    if (target_context) {
        glfwMakeContextCurrent(target_context);
    } else {
        LOG_WARN("[OpenGL RHI] Shutting down without a valid OpenGL context! Skipping VRAM cleanup to prevent segfaults.");
    }

    // Safely delete all GPU resources
    while (!ctx.materials.empty())      DestroyMaterial(registry, ctx.materials.begin()->first);
    while (!ctx.meshes.empty())         DestroyMesh(registry, ctx.meshes.begin()->first);
    while (!ctx.textures.empty())       DestroyTexture(registry, ctx.textures.begin()->first);
    while (!ctx.render_targets.empty()) DestroyRenderTarget(registry, ctx.render_targets.begin()->first);

    // Clean the UI VAO
    if (ctx.ui_empty_vao != 0) {
        if (glfwGetCurrentContext() != nullptr) {
            glDeleteVertexArrays(1, &ctx.ui_empty_vao);
        }
        ctx.ui_empty_vao = 0;
    }
    
    // Clean shaders
    ShaderManager::I().ClearShaders(); 

    // Destroy the hidden offscreen window if it exists
    if (ctx.hidden_window) {
        glfwDestroyWindow(ctx.hidden_window);
        ctx.hidden_window = nullptr;
    }

    glfwMakeContextCurrent(nullptr);
    registry.ctx().erase<OpenGLContext>();
    LOG_INFO("[OpenGL RHI] System shutdown complete.");
}
static void RenderFrame(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    
    // Lambda to actually bind a shader by its name
    Shader* active_shader = nullptr;
    auto bind_shader = [&](rrl::asset::ShadingModel model) -> Shader* {
        Shader* target = ShaderManager::I().GetShader(model);
        if (target) {
            if (target != active_shader) {
                target->Use();
                active_shader = target;
            }
            return target;
        }
        return nullptr;
    };

    // 3D Scene Rendering
    auto mesh_view = registry.view<tf::TFWorldTransformComponent, rrl::asset::MeshLinkage>();
    auto cam_view = registry.view<camera::CameraComponent, camera::CameraRuntimeComponent>();
    cam_view.use<camera::CameraComponent>(); // Use CameraComponent as iteration drive to used the sorted view.
    for (auto cam_entity : cam_view) {
        const auto& cam = cam_view.get<camera::CameraComponent>(cam_entity);
        const auto& cam_rt = cam_view.get<camera::CameraRuntimeComponent>(cam_entity);

        auto target_it = ctx.render_targets.find(cam.target_fbo);
        if (target_it == ctx.render_targets.end()) continue;
        
        // Bind offscreen canvas target
        glBindFramebuffer(GL_FRAMEBUFFER, target_it->second.fbo);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, target_it->second.width, target_it->second.height);

        for (auto physical_entity : mesh_view) {
            const auto& linkage = mesh_view.get<rrl::asset::MeshLinkage>(physical_entity);
            const auto& world_tf = mesh_view.get<tf::TFWorldTransformComponent>(physical_entity);
            
            if (!registry.valid(linkage.mesh_asset) || !registry.all_of<rrl::asset::MeshRuntimeComponent>(linkage.mesh_asset)) continue;
            if ((cam.culling_mask & linkage.layer_mask) == rhi::RHIRenderLayer::LAYER_NONE) continue;

            MeshHandle mesh_handle = registry.get<rrl::asset::MeshRuntimeComponent>(linkage.mesh_asset).handle;
            if (ctx.meshes.find(mesh_handle) == ctx.meshes.end()) continue;
            const GLMesh& gl_mesh = ctx.meshes[mesh_handle];

            glm::mat4 mvp = cam_rt.view_projection_matrix * world_tf.matrix;
            glBindVertexArray(gl_mesh.vao);

            // Point Clouds (LiDAR)
            if (gl_mesh.topology == GL_POINTS) {
                Shader* pc_shader = bind_shader(rrl::asset::ShadingModel::POINT_CLOUD);
                if (pc_shader) {
                    pc_shader->SetMat4("u_MVP", mvp);
                    glDrawArrays(gl_mesh.topology, 0, gl_mesh.vertex_count);
                }
                continue;
            }

            // Triangles / Lines
            std::vector<rrl::asset::MeshSubmesh> default_submesh = {{0, gl_mesh.index_count}};
            const std::vector<rrl::asset::MeshSubmesh>& active_submeshes = gl_mesh.submeshes.empty() ? default_submesh : gl_mesh.submeshes;
            for (size_t i = 0; i < active_submeshes.size(); ++i) {
                const auto& submesh = active_submeshes[i];

                // Fetch material from the linkage
                entt::entity mat_entity = entt::null;
                if (i < linkage.materials.size()) {
                    mat_entity = linkage.materials[i];
                }
                auto shading_model = rrl::asset::ShadingModel::UNLIT; 
                if (registry.valid(mat_entity) && registry.all_of<rrl::asset::MaterialSourceComponent>(mat_entity)) {
                    shading_model = registry.get<rrl::asset::MaterialSourceComponent>(mat_entity).data.shading_model;
                }
        
                Shader* mat_shader = bind_shader(shading_model);
                if (!mat_shader) continue;
                
                mat_shader->SetMat4("u_MVP", mvp);

                if (registry.valid(mat_entity) && registry.all_of<rrl::asset::MaterialRuntimeComponent>(mat_entity)) {
                    const auto& mat_rt = registry.get<rrl::asset::MaterialRuntimeComponent>(mat_entity);
                    
                    auto mat_it = ctx.materials.find(mat_rt.handle);
                    if (mat_it != ctx.materials.end()) {
                        glBindBufferBase(GL_UNIFORM_BUFFER, 0, mat_it->second.ubo);
                    }

                    if (mat_rt.albedo_handle != rhi::TEXTURE_NULL && ctx.textures.find(mat_rt.albedo_handle) != ctx.textures.end()) {
                        glBindTextureUnit(0, ctx.textures[mat_rt.albedo_handle].id);
                        mat_shader->SetInt("u_HasAlbedo", 1);
                    } else {
                        mat_shader->SetInt("u_HasAlbedo", 0);
                    }
                }

                glDrawElements(gl_mesh.topology, submesh.index_count, GL_UNSIGNED_INT, (void*)(submesh.index_offset * sizeof(uint32_t)));
            }
        }
    }

    // 2D UI Rendering
    auto target_main_it = ctx.render_targets.find(TARGET_MAIN);
    if (target_main_it != ctx.render_targets.end()) {
        const GLRenderTarget& main_rt = target_main_it->second;
        glBindFramebuffer(GL_FRAMEBUFFER, main_rt.fbo);
        glViewport(0, 0, main_rt.width, main_rt.height);
    }

    glDisable(GL_DEPTH_TEST); 
    glEnable(GL_BLEND);       
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader* ui_shader = bind_shader(rrl::asset::ShadingModel::UI2D);
    if (ui_shader) {
        glBindVertexArray(ctx.ui_empty_vao);

        auto ui_view = registry.view<rrl::asset::TextureLinkage>();
        for (auto ui_entity : ui_view) {
            const auto& linkage = ui_view.get<rrl::asset::TextureLinkage>(ui_entity);
            
            if (!registry.valid(linkage.texture_asset) || !registry.all_of<rrl::asset::TextureRuntimeComponent>(linkage.texture_asset)) continue;
            if ((linkage.layer_mask & rhi::RHIRenderLayer::LAYER_UI) == rhi::RHIRenderLayer::LAYER_NONE) continue;
            
            rhi::TextureHandle tex_handle = registry.get<rrl::asset::TextureRuntimeComponent>(linkage.texture_asset).handle;
            auto tex_it = ctx.textures.find(tex_handle);
            if (tex_it == ctx.textures.end()) continue;

            glBindTextureUnit(0, tex_it->second.id);
            
            ui_shader->SetFloat("u_X", linkage.screen_x);
            ui_shader->SetFloat("u_Y", linkage.screen_y);
            ui_shader->SetFloat("u_W", linkage.screen_w);
            ui_shader->SetFloat("u_H", linkage.screen_h);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    // Restore Default 3D State
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



// --- Render Targets (FBOs) ---------------------------------------
static RenderTargetHandle CreateRenderTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();
    
    GLRenderTarget rt;
    rt.width = width;
    rt.height = height;

    // FBO via DSA
    glCreateFramebuffers(1, &rt.fbo);

    // Create and attach Color Texture (RGB 8-bit)
    glCreateTextures(GL_TEXTURE_2D, 1, &rt.color_texture);
    glTextureStorage2D(rt.color_texture, 1, GL_RGB8, width, height);
    glNamedFramebufferTexture(rt.fbo, GL_COLOR_ATTACHMENT0, rt.color_texture, 0);

    // Create and attach Combined Depth-Stencil Texture (24-bit Depth, 8-bit Stencil)
    glCreateTextures(GL_TEXTURE_2D, 1, &rt.depth_texture);
    glTextureStorage2D(rt.depth_texture, 1, GL_DEPTH24_STENCIL8, width, height);
    glNamedFramebufferTexture(rt.fbo, GL_DEPTH_STENCIL_ATTACHMENT, rt.depth_texture, 0);

    RRL_ASSERT(glCheckNamedFramebufferStatus(rt.fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, 
               "[OpenGL RHI] Failed to create complete Framebuffer Object.");

    RenderTargetHandle handle = ctx.next_target_handle++;
    ctx.render_targets[handle] = rt;
    return handle;
}

static void DestroyRenderTarget(entt::registry& registry, RenderTargetHandle handle) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();

    auto it = ctx.render_targets.find(handle);
    if (it != ctx.render_targets.end()) {
        if (glfwGetCurrentContext() != nullptr) {
            glDeleteFramebuffers(1, &it->second.fbo);
            glDeleteTextures(1, &it->second.color_texture);
            glDeleteTextures(1, &it->second.depth_texture);
        }
        ctx.render_targets.erase(it);
    }
}


// --- Textures ----------------------------------------------------
static void UpdateTexture(entt::registry& registry, TextureHandle handle, const rrl::asset::ImageAsset& image_data) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();

    auto it = ctx.textures.find(handle);
    if (it == ctx.textures.end() || image_data.data.empty()) return;
    GLTexture& tex = it->second;

    // Ensure the incoming data matches the GPU allocation
    RRL_ASSERT(image_data.width == tex.width && image_data.height == tex.height, 
               "[OpenGL RHI] UpdateTexture rejected! Changing dimensions at runtime is forbidden for performance.");

    // Direct DMA transfer from System RAM to VRAM. 
    glTextureSubImage2D(tex.id, 0, 0, 0, tex.width, tex.height, tex.upload_format, tex.upload_type, image_data.data.data());
}
static TextureHandle CreateTexture(entt::registry& registry, const rrl::asset::ImageAsset& image_data) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();
    
    if (!image_data.IsImageModelValid()) return TEXTURE_NULL;

    GLTexture tex;
    tex.width = image_data.width;
    tex.height = image_data.height;

    // Map formats 
    GLenum gl_internal_format = GL_RGB8;
    tex.upload_format = GL_RGB;
    tex.upload_type = GL_UNSIGNED_BYTE;
    if (image_data.channels == rrl::asset::ImageChannelLayout::CH_4) {
        gl_internal_format = (image_data.data_type == rrl::asset::ImageAssetType::FLOAT32) ? GL_RGBA32F : GL_RGBA8;
        tex.upload_format = (image_data.color_layout == rrl::asset::ImageColorLayout::BGRA) ? GL_BGRA : GL_RGBA;
    } else if (image_data.channels == rrl::asset::ImageChannelLayout::CH_1) {
        gl_internal_format = (image_data.data_type == rrl::asset::ImageAssetType::FLOAT32) ? GL_R32F : GL_R8;
        tex.upload_format = GL_RED;
    }
    if (image_data.color_layout == rrl::asset::ImageColorLayout::BGR) tex.upload_format = GL_BGR;
    if (image_data.data_type == rrl::asset::ImageAssetType::FLOAT32) tex.upload_type = GL_FLOAT;

    // Allocate Immutable VRAM Storage via DSA
    glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
    glTextureStorage2D(tex.id, 1, gl_internal_format, tex.width, tex.height);
    
    // Filtering for general 3D assets (can be bypassed for pure pixel-perfect UI later)
    GLint gl_filter = (image_data.filter == rrl::asset::ImageFilter::NEAREST) ? GL_NEAREST : GL_LINEAR;
    glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, gl_filter);
    glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, gl_filter);
    glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Save state and upload data
    TextureHandle handle = ctx.next_tex_handle++;
    ctx.textures[handle] = tex;
    UpdateTexture(registry, handle, image_data);
    return handle;
}
static void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();
    auto it = ctx.textures.find(handle);
    if (it != ctx.textures.end()) {
        if (glfwGetCurrentContext() != nullptr) {
            glDeleteTextures(1, &it->second.id);
        }
        ctx.textures.erase(it);
    }
}


// --- Meshes ------------------------------------------------------
static MeshHandle CreateMesh(entt::registry& registry, const rrl::asset::MeshAsset& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();
    
    if (mesh_data.positions.empty()) return MESH_NULL;

    GLMesh gl_mesh;
    gl_mesh.topology        = MapTopology(mesh_data.topology);
    gl_mesh.vertex_count    = static_cast<uint32_t>(mesh_data.positions.size());
    gl_mesh.index_count     = static_cast<uint32_t>(mesh_data.indices.size());
    gl_mesh.vertex_capacity = gl_mesh.vertex_count;
    gl_mesh.index_capacity  = gl_mesh.index_count;
    gl_mesh.submeshes       = mesh_data.submeshes;


    // Create VAO via DSA
    glCreateVertexArrays(1, &gl_mesh.vao);

    // Lambda to create a VBO only if the CPU object is populated
    auto setup_vbo = [&](GLuint& vbo, GLuint shader_location, GLint components, const auto& data_vec) {
        if (data_vec.empty()) return;
        
        glCreateBuffers(1, &vbo);
        // GL_DYNAMIC_DRAW allows blazing-fast point cloud streaming updates
        glNamedBufferData(vbo, data_vec.size() * sizeof(data_vec[0]), data_vec.data(), GL_DYNAMIC_DRAW);
        
        // Link VBO to VAO
        glVertexArrayVertexBuffer(gl_mesh.vao, shader_location, vbo, 0, sizeof(data_vec[0]));
        glEnableVertexArrayAttrib(gl_mesh.vao, shader_location);
        glVertexArrayAttribFormat(gl_mesh.vao, shader_location, components, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(gl_mesh.vao, shader_location, shader_location);
    };

    // Map SoA vectors to Shader Locations
    // (Location 0: Pos, Location 1: Norm, Location 2: UV, Location 3: Color)
    setup_vbo(gl_mesh.vbo_positions, 0, 3, mesh_data.positions);
    setup_vbo(gl_mesh.vbo_normals,   1, 3, mesh_data.normals);
    setup_vbo(gl_mesh.vbo_uvs,       2, 2, mesh_data.uvs);
    setup_vbo(gl_mesh.vbo_colors,    3, 4, mesh_data.colors);

    // Setup Element Buffer (Indices)
    if (!mesh_data.indices.empty()) {
        glCreateBuffers(1, &gl_mesh.ebo);
        glNamedBufferData(gl_mesh.ebo, mesh_data.indices.size() * sizeof(uint32_t), mesh_data.indices.data(), GL_STATIC_DRAW);
        glVertexArrayElementBuffer(gl_mesh.vao, gl_mesh.ebo);
    }

    MeshHandle handle = ctx.next_mesh_handle++;
    ctx.meshes[handle] = gl_mesh;
    return handle;
}
static void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();
    
    auto it = ctx.meshes.find(handle);
    if (it != ctx.meshes.end()) {
        if (glfwGetCurrentContext() != nullptr) {
            const GLMesh& m = it->second;
            if (m.vao)              glDeleteVertexArrays(1, &m.vao);
            if (m.vbo_positions)    glDeleteBuffers(1, &m.vbo_positions);
            if (m.vbo_normals)      glDeleteBuffers(1, &m.vbo_normals);
            if (m.vbo_uvs)          glDeleteBuffers(1, &m.vbo_uvs);
            if (m.vbo_colors)       glDeleteBuffers(1, &m.vbo_colors);
            if (m.ebo)              glDeleteBuffers(1, &m.ebo);
        }
        
        ctx.meshes.erase(it);
    }
}
static void UpdateMesh(entt::registry& registry, MeshHandle handle, const rrl::asset::MeshAsset& mesh_data) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();

    auto it = ctx.meshes.find(handle);
    if (it == ctx.meshes.end()) return;
    GLMesh& gl_mesh = it->second;

    uint32_t new_v_count = static_cast<uint32_t>(mesh_data.positions.size());
    uint32_t new_i_count = static_cast<uint32_t>(mesh_data.indices.size());

    // Ensure we aren't trying to update a VBO that was never created
    bool missing_vbos = 
        (gl_mesh.vbo_normals == 0 && !mesh_data.normals.empty()) ||
        (gl_mesh.vbo_uvs == 0     && !mesh_data.uvs.empty()) ||
        (gl_mesh.vbo_colors == 0  && !mesh_data.colors.empty()) ||
        (gl_mesh.ebo == 0         && !mesh_data.indices.empty());

    // required VBOs and enough VRAM capacity
    if (!missing_vbos && new_v_count <= gl_mesh.vertex_capacity && new_i_count <= gl_mesh.index_capacity) {
        auto update_vbo = [](GLuint vbo, const auto& data_vec) {
            if (vbo != 0 && !data_vec.empty()) {
                glNamedBufferSubData(vbo, 0, data_vec.size() * sizeof(data_vec[0]), data_vec.data());
            }
        };

        // Stream the new data directly to the GPU
        update_vbo(gl_mesh.vbo_positions, mesh_data.positions);
        update_vbo(gl_mesh.vbo_normals, mesh_data.normals);
        update_vbo(gl_mesh.vbo_uvs, mesh_data.uvs);
        update_vbo(gl_mesh.vbo_colors, mesh_data.colors);

        // Update indices (EBO)
        if (gl_mesh.ebo != 0 && !mesh_data.indices.empty()) {
            glNamedBufferSubData(gl_mesh.ebo, 0, mesh_data.indices.size() * sizeof(uint32_t), mesh_data.indices.data());
        }

        gl_mesh.vertex_count = new_v_count;
        gl_mesh.index_count = new_i_count;
        gl_mesh.submeshes   = mesh_data.submeshes;
        gl_mesh.topology    = MapTopology(mesh_data.topology);
        
        return; 
    }

    // Buffer needs to grow or layout changed. Reallocate.
    DestroyMesh(registry, handle);
    ctx.next_mesh_handle--; 
    CreateMesh(registry, mesh_data);
}



// --- Materials ---------------------------------------------------
static void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const rrl::asset::MaterialAsset& material_data) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();

    auto it = ctx.materials.find(handle);
    if (it == ctx.materials.end()) return;
    GLMaterial& gl_mat = it->second;

    // Pack the raw data into the GPU-aligned struct
    MaterialUBOData ubo_data;
    ubo_data.base_color = material_data.base_color;
    ubo_data.emission   = material_data.emmission;
    ubo_data.roughness  = material_data.roughness;
    ubo_data.metallic   = material_data.metallic;

    // Direct DMA transfer to the VRAM buffer
    glNamedBufferSubData(gl_mat.ubo, 0, sizeof(MaterialUBOData), &ubo_data);
}
static MaterialHandle CreateMaterial(entt::registry& registry, const rrl::asset::MaterialAsset& material_data) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();

    GLMaterial gl_mat;
    glCreateBuffers(1, &gl_mat.ubo);
    
    // Allocate the memory block on the GPU using GL_DYNAMIC_DRAW (fast material data updates)
    glNamedBufferData(gl_mat.ubo, sizeof(MaterialUBOData), nullptr, GL_DYNAMIC_DRAW);
    MaterialHandle handle = ctx.next_mat_handle++;
    ctx.materials[handle] = gl_mat;
    UpdateMaterial(registry, handle, material_data);
    return handle;
}
static void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();
    
    auto it = ctx.materials.find(handle);
    if (it != ctx.materials.end()) {
        if (glfwGetCurrentContext() != nullptr) {
            glDeleteBuffers(1, &it->second.ubo);
        }
        ctx.materials.erase(it);
    }
}



// --- Presentation ------------------------------------------------
static rrl::asset::ImageAsset GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();

    auto it = ctx.render_targets.find(handle);
    if (it == ctx.render_targets.end()) return {};

    const auto& rt = it->second;

    rrl::asset::ImageAsset img;
    img.width           = rt.width;
    img.height          = rt.height;
    img.channels        = rrl::asset::ImageChannelLayout::CH_3;
    img.color_layout    = rrl::asset::ImageColorLayout::RGB;
    img.data_type       = rrl::asset::ImageAssetType::UINT8;
    img.origin          = rrl::asset::ImageOrigin::BOTTOM_LEFT;
    img.data.resize(rt.width * rt.height * 3);

    // DSA Pixel Read
    glGetTextureImage(rt.color_texture, 0, GL_RGB, GL_UNSIGNED_BYTE, img.data.size(), img.data.data());
    return img;
}
static void Present(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();

    if (!ctx.active_window) return;

    // OpenCV Window Presentation Support
    if (ctx.active_window->type == RHIWindowType::OPENCV) {
        #ifdef RRL_BUILD_WINDOW_OPENCV
        const char* win_name = static_cast<const char*>(ctx.active_window->native_handle);
        if (!win_name) return;

        // Fetch data from GPU
        rrl::asset::ImageAsset raw_img = GetTargetImage(registry, TARGET_MAIN);
        if (raw_img.data.empty()) return;

        // Wrap the raw buffer
        int cv_type = CV_8UC3;
        if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_4) cv_type = CV_8UC4;
        else if (raw_img.channels == rrl::asset::ImageChannelLayout::CH_1) cv_type = CV_8UC1;
        cv::Mat fbo_mat(raw_img.height, raw_img.width, cv_type, raw_img.data.data());
        cv::Mat ready_mat;

        // Coordinate origin flipping
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

        // Scale to viewport and Present
        if (ctx.active_window->width != ctx.render_width || ctx.active_window->height != ctx.render_height) {
            cv::Mat scaled_mat;
            cv::resize(final_mat, scaled_mat, cv::Size(ctx.active_window->width, ctx.active_window->height), 0, 0, cv::INTER_NEAREST);
            cv::imshow(win_name, scaled_mat);
        } else {
            cv::imshow(win_name, final_mat);
        }

        #else
        LOG_ERROR("[OpenGL RHI] Present called for OpenCV window, but OpenCV support was not compiled.");
        #endif
        return;
    }

    // GLFW Window Presentation Support
    else if (ctx.active_window->type == RHIWindowType::GLFW) {
        GLFWwindow* gl_window = static_cast<GLFWwindow*>(ctx.active_window->native_handle);
        if (!gl_window) return;

        // Fetch our rendered offscreen canvas
        auto it = ctx.render_targets.find(TARGET_MAIN);
        if (it != ctx.render_targets.end()) {
            const GLRenderTarget& main_rt = it->second;

            // Fetch actual OS window size
            int win_w, win_h;
            glfwGetFramebufferSize(gl_window, &win_w, &win_h);

            // Blit TARGET_MAIN to the OS Window (Framebuffer 0)
            glBlitNamedFramebuffer(
                main_rt.fbo, 0, // Source FBO, Dest FBO
                0, 0, main_rt.width, main_rt.height, // Source Rect
                0, 0, win_w, win_h,                  // Dest Rect
                GL_COLOR_BUFFER_BIT, GL_NEAREST
            );
        }

        glfwSwapBuffers(gl_window);
        return;
    }
}
static void OnWindowDestroyed(entt::registry& registry, const RHIWindow* window) {
    if (!registry.ctx().contains<OpenGLContext>()) return;
    auto& ctx = registry.ctx().get<OpenGLContext>();

    if (ctx.active_window == window) {
        LOG_WARN("[OpenGL RHI] Active presentation window destroyed by application. Detaching from it.");
        // Clear the active context!!
        ctx.active_window = nullptr;
        glfwMakeContextCurrent(nullptr);
    }
}





// --- Creation ----------------------------------------------------
RHIBackend CreateOpenGLBackend() {
    RHIBackend backend;
    backend.type                = RHIBackendType::OPENGL;

    // Lifecycle
    backend.Initialize          = Initialize;
    backend.Shutdown            = Shutdown;
    backend.RenderFrame         = RenderFrame;

    // Target FBOs
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

    // Presentation
    backend.GetTargetImage      = GetTargetImage;
    backend.Present             = Present;
    backend.OnWindowDestroyed   = OnWindowDestroyed;

    

    // ----- Placeholders


    // Debugging
    backend.SetDebugFlag        = [](entt::registry& registry, RHIDebugFlag flag, bool enable) {  };
    backend.GetActiveDebugFlags = [](entt::registry& registry) { return RHIDebugFlag::FLAG_NONE; };


    return backend;
}


} // namespace rrl::rhi::opengl
    