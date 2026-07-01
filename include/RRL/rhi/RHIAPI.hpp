// RRL/include/rhi/RHIAPI.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"

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
 * @brief Synchronizes all modified CPU data components (Textures, Meshes, Materials) 
 * with their respective GPU/RHI hardware handles.
 * Should be called once per frame, right before rhi::RenderFrame.
 */
void SyncResources(entt::registry& registry);

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


// --- Textures ----------------------------------------------------
/**
 * @brief Requests the backend to allocate and create a texture.
 */
TextureHandle CreateTexture(entt::registry& registry, const data::ImageData& image_data);
/**
 * @brief Updates an already allocated texture.
 */
void UpdateTexture(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data);
/**
 * @brief Destroys a texture freeing its memory.
 */
void DestroyTexture(entt::registry& registry, TextureHandle handle);


// --- Meshes ------------------------------------------------------
/**
 * @brief Requests the backend to allocate and create a mesh.
 */
MeshHandle CreateMesh(entt::registry& registry, const data::MeshData& mesh_data);
/**
 * @brief Updates an already allocated mesh.
 */
void UpdateMesh(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data);
/**
 * @brief Destroys a mesh freeing its memory.
 */
void DestroyMesh(entt::registry& registry, MeshHandle handle);


// --- Materials ---------------------------------------------------
/**
 * @brief Requests the backend to allocate and create a new material.
 */
MaterialHandle CreateMaterial(entt::registry& registry, const data::MaterialData& material_data);
/**
 * @brief Updates an already allocated material.
 */
void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data);
/**
 * @brief Destroys a material freeing its memory.
 */
void DestroyMaterial(entt::registry& registry, MaterialHandle handle);


// --- Presentation ------------------------------------------------
/**
 * @brief Reads rendered data from the RHI back to CPU RAM.
 */
data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle);
/**
 * @brief Asks the backend to swap its TARGET_MAIN buffer to the attached window.
 */
void Present(entt::registry& registry);


} // namespace rrl::rhi 

