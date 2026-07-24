// RRL/src/rhi/RHI.cpp

#include "RRL/rhi/RHI.hpp"
#include "RRL/asset/ImageAsset.hpp"
#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/rhi/RHIBackendManager.hpp"

#include "RRL/asset/SynchronizationSystems.hpp"

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
    rrl::asset::SyncTexturesToRHI(registry);
    rrl::asset::SyncMeshesToRHI(registry);
    rrl::asset::SyncMaterialsToRHI(registry);
}
void RenderFrame(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.RenderFrame != nullptr, "RHI RenderFrame called but no backend is loaded!");

    SyncResources(registry);
    backend.RenderFrame(registry);
    Present(registry);
}



// --- Render Targets (FBOs) ---------------------------------------
void CreateRenderTarget(entt::registry& registry, ResourceID id, const rrl::rhi::RenderTargetDescriptor& desc) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateRenderTarget != nullptr, "RHI CreateRenderTarget called but no backend is loaded!");
    
    rrl::rhi::PhysicalRenderTargetDescriptor phys_desc;
    phys_desc.width = desc.width;
    phys_desc.height = desc.height;
    phys_desc.is_texture_array = desc.is_texture_array;
    phys_desc.array_idx = desc.array_idx;

    // Auto-allocate or Resolve Color Attachments
    for (ResourceID color_id : desc.color_attachments) {
        TextureHandle tex_handle = backend.cache.GetPhysicalTexture(color_id);
        
        if (tex_handle == BACKEND_TEXTURE_NULL) {
            // Lazy Allocation: User requested an ID that doesn't exist. Allocate an empty FBO texture for them.
            // Defaulting to UINT8 for color FBOs
            tex_handle = backend.CreateRenderTexture(registry, desc.width, desc.height, 
                                                     rrl::asset::ImageAssetType::UINT8, 
                                                     rrl::asset::ImageChannelLayout::CH_4, 
                                                     false, 1);
            backend.cache.RegisterTexture(color_id, tex_handle);
        }
        phys_desc.color_attachments.push_back(tex_handle);
    }

    // Auto-allocate or Resolve Depth Attachment
    if (desc.depth_stencil_attachment != RESOURCE_NULL) {
        TextureHandle depth_handle = backend.cache.GetPhysicalTexture(desc.depth_stencil_attachment);
        
        if (depth_handle == BACKEND_TEXTURE_NULL) {
            // Lazy Allocation: Allocate a depth/stencil buffer
            depth_handle = backend.CreateRenderTexture(registry, desc.width, desc.height, 
                                                       rrl::asset::ImageAssetType::UINT8, 
                                                       rrl::asset::ImageChannelLayout::CH_1, 
                                                       true, 1);
            backend.cache.RegisterTexture(desc.depth_stencil_attachment, depth_handle);
        }
        phys_desc.depth_stencil_attachment = depth_handle;
    }

    // Dispatch physical creation to the backend
    RenderTargetHandle physical_handle = backend.CreateRenderTarget(registry, phys_desc);
    if (physical_handle != BACKEND_TARGET_NULL) {
        backend.cache.RegisterTarget(id, physical_handle);
    } else {
        LOG_ERROR("Failed to create RenderTarget for ResourceID: {}", id.id);
    }
}
void DestroyRenderTarget(entt::registry& registry, ResourceID id) {
    if (id == TARGET_MAIN) return; // Cannot destroy the main swapchain

    auto& backend = RHIBackendManager::Instance().GetBackend();
    RenderTargetHandle physical_handle = backend.cache.GetPhysicalTarget(id);
    
    if (physical_handle != BACKEND_TARGET_NULL && backend.DestroyRenderTarget != nullptr) {
        backend.DestroyRenderTarget(registry, physical_handle);
        backend.cache.UnregisterTarget(id);
    }
}

// --- Textures ----------------------------------------------------
TextureHandle CreateTexture(entt::registry& registry, ResourceID id, const rrl::asset::ImageAsset& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateTexture != nullptr, "RHI CreateTexture called but no backend is loaded!");
    RRL_ASSERT(image_data.IsValid(), "RHI CreateTexture called but invalid or not populated image was provided");
    
    TextureHandle physical_handle = backend.CreateTexture(registry, image_data);
    if (physical_handle != BACKEND_TEXTURE_NULL) {
        backend.cache.RegisterTexture(id, physical_handle);
    }
    return physical_handle;
}
void UpdateTexture(entt::registry& registry, TextureHandle handle, const rrl::asset::ImageAsset& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateTexture != nullptr, "RHI UpdateTexture called but no backend is loaded!");
    RRL_ASSERT(image_data.IsValid(), "RHI UpdateTexture called but invalid image provided");
    
    if (handle != BACKEND_TEXTURE_NULL) {
        backend.UpdateTexture(registry, handle, image_data);
    }
}
void UpdateTexture(entt::registry& registry, ResourceID id, const rrl::asset::ImageAsset& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateTexture != nullptr, "RHI UpdateTexture called but no backend is loaded!");
    RRL_ASSERT(image_data.IsValid(), "RHI UpdateTexture called but invalid or not populated image was provided");
    
    TextureHandle physical_handle = backend.cache.GetPhysicalTexture(id);
    if (physical_handle != BACKEND_TEXTURE_NULL) {
        backend.UpdateTexture(registry, physical_handle, image_data);
    } else {
        LOG_WARN("Attempted to update a texture that doesn't exist on the GPU (ID: {})", id.id);
    }
}
void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    if (handle == BACKEND_TEXTURE_NULL) return;

    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyTexture != nullptr) {
        backend.DestroyTexture(registry, handle);
        
        ResourceID mapped_id = backend.cache.GetVirtualTexture(handle);
        if (mapped_id != RESOURCE_NULL) {
            backend.cache.UnregisterTexture(mapped_id);
        }
    }
}
void DestroyTexture(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    TextureHandle physical_handle = backend.cache.GetPhysicalTexture(id);
    
    if (physical_handle != BACKEND_TEXTURE_NULL && backend.DestroyTexture != nullptr) {
        backend.DestroyTexture(registry, physical_handle);
        backend.cache.UnregisterTexture(id);
    }
}


// --- Meshes ------------------------------------------------------
MeshHandle CreateMesh(entt::registry& registry, ResourceID id, const rrl::asset::MeshAsset& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateMesh != nullptr, "RHI CreateMesh called but no backend is loaded!");
    
    MeshHandle physical_handle = backend.CreateMesh(registry, mesh_data);
    if (physical_handle != BACKEND_MESH_NULL) {
        backend.cache.RegisterMesh(id, physical_handle);
    }
    return physical_handle;
}
void UpdateMesh(entt::registry& registry, MeshHandle handle, const rrl::asset::MeshAsset& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMesh != nullptr, "RHI UpdateMesh called but no backend is loaded!");
    
    if (handle != BACKEND_MESH_NULL) {
        backend.UpdateMesh(registry, handle, mesh_data);
    }
}
void UpdateMesh(entt::registry& registry, ResourceID id, const rrl::asset::MeshAsset& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMesh != nullptr, "RHI UpdateMesh called but no backend is loaded!");
    
    MeshHandle physical_handle = backend.cache.GetPhysicalMesh(id);
    if (physical_handle != BACKEND_MESH_NULL) {
        backend.UpdateMesh(registry, physical_handle, mesh_data);
    } else {
        LOG_WARN("Attempted to update a virtual mesh that doesn't exist on the GPU (ID: {})", id.id);
    }
}
void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    if (handle == BACKEND_MESH_NULL) return;

    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyMesh != nullptr) {
        backend.DestroyMesh(registry, handle);
        
        ResourceID mapped_id = backend.cache.GetVirtualMesh(handle);
        if (mapped_id != RESOURCE_NULL) {
            backend.cache.UnregisterMesh(mapped_id);
        }
    }
}
void DestroyMesh(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    MeshHandle physical_handle = backend.cache.GetPhysicalMesh(id);
    
    if (physical_handle != BACKEND_MESH_NULL && backend.DestroyMesh != nullptr) {
        backend.DestroyMesh(registry, physical_handle);
        backend.cache.UnregisterMesh(id);
    }
}


// --- Materials ---------------------------------------------------
MaterialHandle CreateMaterial(entt::registry& registry, ResourceID id, const rrl::asset::MaterialAsset& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateMaterial != nullptr, "RHI CreateMaterial called but no backend is loaded!");
    
    MaterialHandle physical_handle = backend.CreateMaterial(registry, material_data);
    if (physical_handle != BACKEND_MATERIAL_NULL) {
        backend.cache.RegisterMaterial(id, physical_handle);
    }
    return physical_handle;
}
void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const rrl::asset::MaterialAsset& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMaterial != nullptr, "RHI UpdateMaterial called but no backend is loaded!");

    if (handle != BACKEND_MATERIAL_NULL) {
        backend.UpdateMaterial(registry, handle, material_data);
    }
}
void UpdateMaterial(entt::registry& registry, ResourceID id, const rrl::asset::MaterialAsset& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMaterial != nullptr, "RHI UpdateMaterial called but no backend is loaded!");

    MaterialHandle physical_handle = backend.cache.GetPhysicalMaterial(id);
    if (physical_handle != BACKEND_MATERIAL_NULL) {
        backend.UpdateMaterial(registry, physical_handle, material_data);
    }
}
void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    if (handle == BACKEND_MATERIAL_NULL) return;

    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyMaterial != nullptr) {
        backend.DestroyMaterial(registry, handle);
        
        ResourceID mapped_id = backend.cache.GetVirtualMaterial(handle);
        if (mapped_id != RESOURCE_NULL) {
            backend.cache.UnregisterMaterial(mapped_id);
        }
    }
}
void DestroyMaterial(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    
    MaterialHandle physical_handle = backend.cache.GetPhysicalMaterial(id);
    if (physical_handle != BACKEND_MATERIAL_NULL && backend.DestroyMaterial != nullptr) {
        backend.DestroyMaterial(registry, physical_handle);
        backend.cache.UnregisterMaterial(id);
    }else {
        LOG_WARN("Attempted to update a virtual material that doesn't exist on the GPU (ID: {})", id.id);
    }
}


// --- Retrieve Rendered Data --------------------------------------
rrl::asset::ImageAsset GetTargetImage(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RenderTargetHandle physical_handle = backend.cache.GetPhysicalTarget(id);
    if (physical_handle != BACKEND_TARGET_NULL && backend.GetTargetImage != nullptr) {
        return backend.GetTargetImage(registry, physical_handle);
    }
    return rrl::asset::ImageAsset{}; // Return empty image
}
void Present(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.Present != nullptr) {
        backend.Present(registry);
    }
}


} // namespace rrl::rhi

