// RRL/src/modules/RHIModule.cpp

#include "RRL/modules/RHIModule.hpp"
#include "RRL/EngineContext.hpp"

#include "RRL/rhi/RHI.hpp"




namespace rrl {

// --- Constructor / Destructor ------------------------------------
RHIModule::RHIModule(EngineContext* ctx): m_ctx(ctx) {
}
RHIModule::~RHIModule() {
    if (m_ctx != nullptr) {
        Shutdown();
    }
}



// --- Window ------------------------------------------------------
rrl::rhi::RHIWindow RHIModule::CreateWindow(rrl::rhi::RHIWindowType window_type) {
    return rrl::rhi::CreateWindow(window_type);
}

bool RHIModule::InitializeWindow(rrl::rhi::RHIWindow& window, const char* title, uint32_t w, uint32_t h) {
    return rrl::rhi::InitializeWindow(window, title, w, h);
}

bool RHIModule::PollWindowEvents(rrl::rhi::RHIWindow& window) {
    return rrl::rhi::PollWindowEvents(window);
}

void RHIModule::DestroyWindow(rrl::rhi::RHIWindow& window) {
    rrl::rhi::DestroyWindow(m_ctx->registry, window);
}


// --- Backend -----------------------------------------------------
bool RHIModule::LoadBackend(rrl::rhi::RHIBackendType target_backend) {
    return rrl::rhi::LoadBackend(target_backend, m_ctx->registry);
}

rrl::rhi::RHIBackendType RHIModule::GetCurrentBackendType() {
    return rrl::rhi::GetCurrentBackendType();
}


// --- Lifecycle ---------------------------------------------------
bool RHIModule::Initialize(const rrl::rhi::RHIWindow* window) {
    return rrl::rhi::Initialize(m_ctx->registry, window);
}
bool RHIModule::Initialize(uint32_t render_width, uint32_t render_height, const rrl::rhi::RHIWindow* window) {
    return rrl::rhi::Initialize(m_ctx->registry, render_width, render_height, window);
}
void RHIModule::Shutdown() {
    rrl::rhi::Shutdown(m_ctx->registry);
}
void RHIModule::RenderFrame() {
    rrl::rhi::RenderFrame(m_ctx->registry);
}
rrl::asset::ImageAsset RHIModule::GetTargetImage(rrl::rhi::ResourceID id) {
    return rrl::rhi::GetTargetImage(m_ctx->registry, id);
}


// --- Render Targets (FBOs) ---------------------------------------
void RHIModule::CreateRenderTarget(rrl::rhi::ResourceID id, const rrl::rhi::RenderTargetDescriptor& desc) {
    rrl::rhi::CreateRenderTarget(m_ctx->registry, id, desc);
}
void RHIModule::DestroyRenderTarget(rrl::rhi::ResourceID id) {
    rrl::rhi::DestroyRenderTarget(m_ctx->registry, id);
}



} // namespace rrl


