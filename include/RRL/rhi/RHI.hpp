// RRL/include/rhi/RHI.hpp
#pragma once

#include <entt/entt.hpp>
#include "RRL/rhi/RHITypes.hpp"
#include "RRL/data/ImageData.hpp"


namespace rrl::rhi {

// --- Window ------------------------------------------------------
RHIWindow CreateWindow(RHIWindowType window_type);
bool InitializeWindow(RHIWindow& window, const char* title, uint32_t w, uint32_t h);
bool PollWindowEvents(RHIWindow& window);
/**
 * @brief Destroys a RHI window. This also tells the 
 * backend that the window is no longer available.
 */
void DestroyWindow(entt::registry& registry, RHIWindow& window);


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
RHIBackendType GetCurrentBackendType();



// --- Lifecycle ---------------------------------------------------
/**
 * @brief Initializes the currently loaded rendering backend.
 * Automatically provisions the TARGET_MAIN render target setting 
 * the rendering dimensions to the provided window handle.
 */
bool Initialize(entt::registry& registry, const RHIWindow* window);
/**
 * @brief Initializes the currently loaded rendering backend.
 */
bool Initialize(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window);
/**
 * @brief Cleans up memory and contexts
 */
void Shutdown(entt::registry& registry);
/**
 * @brief Iterates the current scene, projects 3D data, and dispatches draw calls 
 * across all active cameras and render targets.
 */
void RenderFrame(entt::registry& registry);
/**
 * @brief Reads rendered data from the RHI back to CPU RAM.
 */
data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle = TARGET_MAIN);



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






} // namespace rrl::rhi 
