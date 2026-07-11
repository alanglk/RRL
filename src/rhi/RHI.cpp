// RRL/src/rhi/RHI.cpp

#include "RRL/rhi/RHI.hpp"
#include "RRL/rhi/RHI_Internal.hpp"
#include "RRL/rhi/RHIBackendManager.hpp"

#include "RRL/data/SynchronizationSystems.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


// --- Backend Factories -------------------------------------------
// Forward declare the compile-time available Backend factories
namespace rrl::rhi::software { RHIBackend CreateSoftwareBackend(); }
#ifdef RRL_BUILD_BACKEND_OPENGL
namespace rrl::rhi::opengl   { RHIBackend CreateOpenGLBackend(); }
#endif


// --- Window Factories --------------------------------------------
// Forward declare the compile-time available Window factories
#ifdef RRL_BUILD_WINDOW_OPENCV 
namespace rrl::rhi::window::opencv {
    bool Initialize(RHIWindow& window, const char* title, uint32_t w, uint32_t h);
    bool PollEvents(RHIWindow& window);
    void Shutdown(RHIWindow& window);
}
#endif
#ifdef RRL_BUILD_WINDOW_GLFW 
namespace rrl::rhi::window::glfw {
    bool Initialize(RHIWindow& window, const char* title, uint32_t w, uint32_t h);
    bool PollEvents(RHIWindow& window);
    void Shutdown(RHIWindow& window);
}
#endif



    

namespace rrl::rhi {

// --- Window ------------------------------------------------------
RHIWindow CreateWindow(RHIWindowType window_type) {
    RHIWindow win{};
    win.type = window_type;
    return win;
}
bool InitializeWindow(RHIWindow& window, const char* title, uint32_t w, uint32_t h) {
    switch (window.type) {
        case RHIWindowType::HEADLESS:
            window.width = w;
            window.height = h;
            window.native_handle = nullptr;
            return true;

        case RHIWindowType::OPENCV:
            #ifdef RRL_BUILD_WINDOW_OPENCV
            return window::opencv::Initialize(window, title, w, h);
            #else
            LOG_ERROR("InitializeWindow: OpenCV support not compiled in this build.");
            return false;
            #endif

        case RHIWindowType::GLFW:
            #ifdef RRL_BUILD_WINDOW_GLFW
            return window::glfw::Initialize(window, title, w, h);
            #else
            LOG_ERROR("InitializeWindow: GLFW + OpenGL support not compiled in this build.");
            return false;
            #endif
    }
    return false;
}
bool PollWindowEvents(RHIWindow& window) {
    if (window.type != RHIWindowType::HEADLESS) {
        RRL_ASSERT(window.native_handle != nullptr, "PollWindowEvents: Received a non initialized window!");
    }

    switch (window.type) {
        case RHIWindowType::HEADLESS:
            return true;

        case RHIWindowType::OPENCV:
            #ifdef RRL_BUILD_WINDOW_OPENCV
            return window::opencv::PollEvents(window);
            #else
            return false;
            #endif

        case RHIWindowType::GLFW:
            #ifdef RRL_BUILD_WINDOW_GLFW
            return window::glfw::PollEvents(window);
            #else
            return false;
            #endif
    }
    return false;
}
void DestroyWindow(entt::registry& registry, RHIWindow& window) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.type != RHIBackendType::NONE && backend.OnWindowDestroyed != nullptr) {
        backend.OnWindowDestroyed(registry, &window);
    }

    switch (window.type) {
        case RHIWindowType::HEADLESS:
            break;

        case RHIWindowType::OPENCV:
            #ifdef RRL_BUILD_WINDOW_OPENCV
            window::opencv::Shutdown(window);
            #endif
            break;

        case RHIWindowType::GLFW:
            #ifdef RRL_BUILD_WINDOW_GLFW
            window::glfw::Shutdown(window);
            #endif
            break;
    }

    window.native_handle = nullptr;
    window.width = 0;
    window.height = 0;
}



// --- Backend -----------------------------------------------------
bool LoadBackend(RHIBackendType target_backend, entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.type == target_backend) {
        return true; // Already loaded
    }

    // TODO: Implement hot-swapping logic (extract assets -> shutdown -> switch -> push assets)

    switch (target_backend) {
        case RHIBackendType::SOFTWARE:
            RHIBackendManager::Instance().SetBackend(rrl::rhi::software::CreateSoftwareBackend());
            return true;
            
        case RHIBackendType::OPENGL: {
            #ifdef RRL_BUILD_BACKEND_OPENGL
            RHIBackendManager::Instance().SetBackend(rrl::rhi::opengl::CreateOpenGLBackend());
            return true;
            #else
            LOG_ERROR("Loadbackend: OpenGL support not compiled in this build.");
            return false;
            #endif
            
        }
            
        default:
            LOG_ERROR("LoadBackend failed. Unknown or unavailable requested RHI backend.");
            return false;
    }
}
RHIBackendType GetCurrentBackendType() {
    return RHIBackendManager::Instance().GetBackend().type;
}



// --- Lifecycle ---------------------------------------------------
bool Initialize(entt::registry& registry, const RHIWindow* window) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.Initialize != nullptr, "RHI Initialize called but no backend is loaded!");
    RRL_ASSERT(window != nullptr, "RHI Initialize called but no window is provided!");
    return backend.Initialize(registry, window->width, window->height, window);
}
bool Initialize(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.Initialize != nullptr, "RHI Initialize called but no backend is loaded!");
    RRL_ASSERT(window != nullptr, "RHI Initialize called but no window is provided!");
    return backend.Initialize(registry, render_width, render_height, window);
}
void Shutdown(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.Shutdown != nullptr) {
        backend.Shutdown(registry);
    }
    RHIBackendManager::Instance().Reset(); // Reset backend to NONE
}
void SyncResources(entt::registry& registry) {
    data::SyncTexturesToRHI(registry);
    data::SyncMeshesToRHI(registry);
    data::SyncMaterialsToRHI(registry);
}
void RenderFrame(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.RenderFrame != nullptr, "RHI RenderFrame called but no backend is loaded!");

    SyncResources(registry);
    backend.RenderFrame(registry);
    Present(registry);
}



// --- Render Targets (FBOs) ---------------------------------------
RenderTargetHandle CreateRenderTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateRenderTarget != nullptr, "RHI CreateRenderTarget called but no backend is loaded!");
    return backend.CreateRenderTarget(registry, width, height);
}
void DestroyRenderTarget(entt::registry& registry, RenderTargetHandle handle) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyRenderTarget != nullptr) {
        backend.DestroyRenderTarget(registry, handle);
    }
}

// --- Textures ----------------------------------------------------
TextureHandle CreateTexture(entt::registry& registry, const data::ImageData& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateTexture != nullptr, "RHI CreateTexture called but no backend is loaded!");
    return backend.CreateTexture(registry, image_data);
}
void UpdateTexture(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateTexture != nullptr, "RHI UpdateTexture called but no backend is loaded!");
    backend.UpdateTexture(registry, handle, image_data);
}
void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyTexture != nullptr) {
        backend.DestroyTexture(registry, handle);
    }
}


// --- Meshes ------------------------------------------------------
MeshHandle CreateMesh(entt::registry& registry, const data::MeshData& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateMesh != nullptr, "RHI CreateMesh called but no backend is loaded!");
    return backend.CreateMesh(registry, mesh_data);
}

void UpdateMesh(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMesh != nullptr, "RHI UpdateMesh called but no backend is loaded!");
    backend.UpdateMesh(registry, handle, mesh_data);
}

void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyMesh != nullptr) {
        backend.DestroyMesh(registry, handle);
    }
}


// --- Materials ---------------------------------------------------
MaterialHandle CreateMaterial(entt::registry& registry, const data::MaterialData& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateMaterial != nullptr, "RHI CreateMaterial called but no backend is loaded!");
    return backend.CreateMaterial(registry, material_data);
}
void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMaterial != nullptr, "RHI UpdateMaterial called but no backend is loaded!");
    backend.UpdateMaterial(registry, handle, material_data);
}
void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyMaterial != nullptr) {
        backend.DestroyMaterial(registry, handle);
    }
}


// --- Retrieve Rendered Data --------------------------------------
data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.GetTargetImage != nullptr) {
        return backend.GetTargetImage(registry, handle);
    }
    return data::ImageData{}; // Return empty image
}
void Present(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.Present != nullptr) {
        backend.Present(registry);
    }
}


} // namespace rrl::rhi

