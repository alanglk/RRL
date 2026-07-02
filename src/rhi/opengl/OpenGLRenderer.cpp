// RRL/src/rhi/software/SoftwareRenderer.cpp

#include "RRL/data/ImageData.hpp"

#include "RRL/rhi/RHIBackend.hpp"

#include <entt/entt.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"



namespace rrl::rhi::opengl {


/**
 * @brief Holds the running OpenGL hardware rendering backend context.
 */
struct OpenGLContext {
    uint32_t render_width = 0;
    uint32_t render_height = 0;
    const RHIWindow* active_window { nullptr };

    // Offscreen fallback context for HEADLESS and OPENCV modes
    GLFWwindow* hidden_window { nullptr };
};



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
    glfwMakeContextCurrent(target_gl_context);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_ERROR("[OpenGL RHI] GLAD failed to load OpenGL 4.5 Core function pointers.");
        if (ctx.hidden_window) {
            glfwDestroyWindow(ctx.hidden_window);
        }
        registry.ctx().erase<OpenGLContext>();
        return false;
    }

    // Configure global OpenGL configurations default states
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // Set up default viewport configuration
    glViewport(0, 0, render_width, render_height);
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
    }

    // Loop through EnTT registry and glDelete any leaked GPU resources


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
    
    // Core clear command execution
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}



// --- Presentation ------------------------------------------------
static void Present(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<OpenGLContext>(), "[OpenGL RHI] Backend not initialized!");
    auto& ctx = registry.ctx().get<OpenGLContext>();

    if (!ctx.active_window) return;

    if (ctx.active_window->type == RHIWindowType::GLFW) {
        GLFWwindow* gl_window = static_cast<GLFWwindow*>(ctx.active_window->native_handle);
        glfwSwapBuffers(gl_window);
    }
    else if (ctx.active_window->type == RHIWindowType::OPENCV) {
        // Offscreen frame buffers read-back into OpenCV will be written here later
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

    // Lifecycle mappings
    backend.Initialize          = Initialize;
    backend.Shutdown            = Shutdown;
    backend.RenderFrame         = RenderFrame;
    backend.OnWindowDestroyed   = OnWindowDestroyed;
    backend.Present             = Present;

    // Placeholders
    backend.CreateRenderTarget  = [](entt::registry&, uint32_t, uint32_t) { return TARGET_NULL; };
    backend.DestroyRenderTarget = [](entt::registry&, RenderTargetHandle) {};
    backend.CreateTexture       = [](entt::registry&, const data::ImageData&) { return TEXTURE_NULL; };
    backend.UpdateTexture       = [](entt::registry&, TextureHandle, const data::ImageData&) {};
    backend.DestroyTexture      = [](entt::registry&, TextureHandle) {};
    backend.CreateMesh          = [](entt::registry&, const data::MeshData&) { return MESH_NULL; };
    backend.UpdateMesh          = [](entt::registry&, MeshHandle, const data::MeshData&) {};
    backend.DestroyMesh         = [](entt::registry&, MeshHandle) {};
    backend.CreateMaterial      = [](entt::registry&, const data::MaterialData&) { return MATERIAL_NULL; };
    backend.UpdateMaterial      = [](entt::registry&, MaterialHandle, const data::MaterialData&) {};
    backend.DestroyMaterial     = [](entt::registry&, MaterialHandle) {};
    backend.GetTargetImage      = [](entt::registry&, RenderTargetHandle) { return data::ImageData{}; };

    return backend;
}


} // namespace rrl::rhi::opengl
    