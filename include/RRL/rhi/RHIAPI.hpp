// RRL/include/rhi/RHIAPI.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"

namespace rrl::rhi {

// --- Backend -----------------------------------------------------
/**
 * @brief Swaps the active rendering backend at runtime.
 * If a backend is already active, it safely shuts it down first.
 * Returns false if the requested backend is not available.
 */
bool LoadBackend(RHIBackendType target_backend, entt::registry& registry);

/**
 * @brief Returns the currently active backend type.
 */
RHIBackendType GetCurrentBackend();



// --- Lifecycle ---------------------------------------------------
/**
 * @brief Initializes the currently loaded rendering backend.
 * Automatically provisions the TARGET_MAIN render target.
 */
bool Initialize(entt::registry& registry, const RHIConfig& config);

/**
 * @brief Cleans up memory, contexts, and destroys any open windows.
 */
void Shutdown(entt::registry& registry);

/**
 * @brief Iterates the current scene, projects 3D data, and dispatches draw calls 
 * across all active cameras and render targets.
 */
void RenderFrame(entt::registry& registry);



// --- Render Targets (FBOs) ---------------------------------------
/**
 * @brief Requests the backend to create an off-screen render target.
 */
RenderTargetHandle CreateRenderTarget(entt::registry& registry, uint32_t width, uint32_t height);

/**
 * @brief Destroys an off-screen render target and frees its memory.
 * Note: TARGET_MAIN cannot be destroyed through this function.
 */
void DestroyRenderTarget(entt::registry& registry, RenderTargetHandle handle);

/**
 * @brief Reads rendered data from the RHI back to CPU RAM.
 */
data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle);


} // namespace rrl::rhi 

