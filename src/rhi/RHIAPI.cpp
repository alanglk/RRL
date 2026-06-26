// RRL/src/rhi/RHIAPI.cpp

#include "RRL/rhi/RHIAPI.hpp"
#include "RRL/rhi/RHIBackendManager.hpp"

#include "RRL/data/SynchronizationSystems.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


// Forward declare the compile-time available backends
// (do not include heavy headers)
#ifdef RRL_RHI_OPENCV
    namespace rrl::rhi::opencv { RHIBackend CreateBackend(); }
#endif
    

namespace rrl::rhi {


// --- Backend -----------------------------------------------------
bool LoadBackend(RHIBackendType target_backend, entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.type == target_backend) {
        return true; // Already loaded
    }

    switch (target_backend) {
        case RHIBackendType::OPENCV:
        #ifdef RRL_RHI_OPENCV
            RHIBackendManager::Instance().SetBackend(rrl::rhi::opencv::CreateBackend());
            return true;
        #else
            LOG_ERROR("LoadBackend failed. OpenCV RHI backend was not compiled.");
            return false;
        #endif
            
        default: {
            LOG_ERROR("LoadBackend failed. Unknonw or not available requested RHI backend.");
            return false;
        }
    }
    return false;
}
RHIBackendType GetCurrentBackend() {
    return RHIBackendManager::Instance().GetBackend().type;
}



// --- Lifecycle ---------------------------------------------------
bool Initialize(entt::registry& registry, const RHIConfig& config) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.Initialize != nullptr, "RHI Initialize called but no backend is loaded!");
    return backend.Initialize(registry, config);
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
    backend.RenderFrame(registry);
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


} // namespace rrl::rhi

