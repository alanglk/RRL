// RRL/src/rhi/RHIAPI.cpp

#include "RRL/rhi/RHIAPI.hpp"
#include "RRL/rhi/RHIBackend.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


// Forward declare the compile-time available backends
// (do not include heavy headers)
#ifdef RRL_RHI_OPENCV
    namespace rrl::rhi::opencv { RHIBackend CreateBackend(); }
#endif
    

namespace rrl::rhi {


static RHIBackend g_active_backend;



// --- Backend -----------------------------------------------------
bool LoadBackend(RHIBackendType target_backend, entt::registry& registry) {
    
    if (g_active_backend.type == target_backend) {
        return true; // Already loaded
    }

    // Backend loading dispatch
    switch (target_backend) {
        case RHIBackendType::OPENCV:
        #ifdef RRL_RHI_OPENCV
            g_active_backend = rrl::rhi::opencv::CreateBackend();
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
    return g_active_backend.type;
}



// --- Lifecycle ---------------------------------------------------
bool Initialize(entt::registry& registry, const RHIConfig& config) {
    RRL_ASSERT(g_active_backend.Initialize != nullptr, "RHI Initialize called but no backend is loaded!");
    return g_active_backend.Initialize(registry, config);
}
void Shutdown(entt::registry& registry) {
if (g_active_backend.Shutdown != nullptr) {
        g_active_backend.Shutdown(registry);
    }
    g_active_backend = RHIBackend{}; // Reset backend to NONE
}
void RenderFrame(entt::registry& registry) {
    RRL_ASSERT(g_active_backend.RenderFrame != nullptr, "RHI RenderFrame called but no backend is loaded!");
    g_active_backend.RenderFrame(registry);
}



// --- Render Targets (FBOs) ---------------------------------------
RenderTargetHandle CreateRenderTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    RRL_ASSERT(g_active_backend.CreateRenderTarget != nullptr, "RHI CreateRenderTarget called but no backend is loaded!");
    return g_active_backend.CreateRenderTarget(registry, width, height);
}
void DestroyRenderTarget(entt::registry& registry, RenderTargetHandle handle) {
    if (g_active_backend.DestroyRenderTarget != nullptr) {
        g_active_backend.DestroyRenderTarget(registry, handle);
    }
}
data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    if (g_active_backend.GetTargetImage != nullptr) {
        return g_active_backend.GetTargetImage(registry, handle);
    }
    return data::ImageData{}; // Return empty image
}




} // namespace rrl::rhi

