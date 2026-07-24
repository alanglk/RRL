// RRL/src/include/RRL/rhi/RHI.hpp
#pragma once

#include <entt/entt.hpp>

#include "RRL/asset/ImageAsset.hpp"

#include "RRL/rhi/RHITypes.hpp"
#include "RRL/rhi/RHIBackend.hpp"



namespace rrl::rhi {

// --- Window ------------------------------------------------------
/**
 * @brief Create a window template for later instantiation.
 */
RHIWindow CreateWindow(RHIWindowType window_type);
/**
 * @brief Actual window creation
 */
bool InitializeWindow(RHIWindow& window, const char* title, uint32_t w, uint32_t h);
/**
 * @brief Window event polling
 */
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
/**
 * @brief Reads rendered data from the RHI back to CPU RAM.
 */
rrl::asset::ImageAsset GetTargetImage(entt::registry& registry, ResourceID id = TARGET_MAIN);
/**
 * @brief Asks the backend to swap its TARGET_MAIN buffer to the attached window.
 */
void Present(entt::registry& registry);



// --- Render Targets (FBOs) ---------------------------------------
/**
 * @brief Requests the backend to create an off-screen render target.
 */
void CreateRenderTarget(entt::registry& registry, ResourceID id, const rrl::rhi::RenderTargetDescriptor& desc);
/**
 * @brief Destroys an off-screen render target and frees its memory.
 * Note: TARGET_MAIN cannot be destroyed through this function.
 */
void DestroyRenderTarget(entt::registry& registry, ResourceID id);



// --- Textures ----------------------------------------------------
/**
 * @brief Allocates a texture, maps it to the provided Virtual ID, and returns the direct physical handle.
 */
TextureHandle CreateTexture(entt::registry& registry, ResourceID id, const rrl::asset::ImageAsset& image_data);
/**
 * @brief Updates an already allocated texture.
 * [ Direct Physical API ] fast-path for ECS Sync updates.
 */
void UpdateTexture(entt::registry& registry, TextureHandle handle, const rrl::asset::ImageAsset& image_data);
/**
 * @brief Updates an already allocated texture.
 * @note [ Virtualized Graph API ] Slower path for RDG / external lookup.
 */
void UpdateTexture(entt::registry& registry, ResourceID id, const rrl::asset::ImageAsset& image_data);
/**
 * @brief Destroys a texture freeing its memory.
 * [ Direct Physical API ] fast-path for Asset GB updates.
 */
void DestroyTexture(entt::registry& registry, TextureHandle id);
/**
 * @brief Destroys a texture freeing its memory.
 */
void DestroyTexture(entt::registry& registry, ResourceID id);



// --- Meshes ------------------------------------------------------
/**
 * @brief Allocates a mesh, maps it to the provided Virtual ID, and returns the direct physical handle.
 */
MeshHandle CreateMesh(entt::registry& registry, ResourceID id, const rrl::asset::MeshAsset& mesh_data);
/**
 * @brief Updates an already allocated mesh.
 * [ Direct Physical API ] fast-path for ECS Sync updates.
 */
void UpdateMesh(entt::registry& registry, MeshHandle handle, const rrl::asset::MeshAsset& mesh_data);
/**
 * @brief Updates an already allocated mesh.
 * @note [ Virtualized Graph API ] Slower path for RDG / external lookup.
 */
void UpdateMesh(entt::registry& registry, ResourceID id, const rrl::asset::MeshAsset& mesh_data);
/**
 * @brief Destroys a mesh freeing its memory.
 * [ Direct Physical API ] fast-path for Asset GB updates.
 */
void DestroyMesh(entt::registry& registry, MeshHandle id);
/**
 * @brief Destroys a mesh freeing its memory.
 */
void DestroyMesh(entt::registry& registry, ResourceID id);



// --- Materials ---------------------------------------------------
/**
 * @brief Allocates a material UBO, maps it to the Virtual ID, and returns the direct physical handle.
 */
MaterialHandle CreateMaterial(entt::registry& registry, ResourceID id, const rrl::asset::MaterialAsset& material_data);
/**
 * @brief Updates an already allocated material.
 * [ Direct Physical API ] fast-path for ECS Sync updates.
 */
void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const rrl::asset::MaterialAsset& material_data);
/**
 * @brief Updates an already allocated material.
 * @note [ Virtualized Graph API ] Slower path for RDG / external lookup.
 */
void UpdateMaterial(entt::registry& registry, ResourceID id, const rrl::asset::MaterialAsset& material_data);
/**
 * @brief Destroys a material freeing its memory.
 * [ Direct Physical API ] fast-path for Asset GB updates.
 */
void DestroyMaterial(entt::registry& registry, MaterialHandle handle);
/**
 * @brief Destroys a material freeing its memory.
 * @note [ Virtualized Graph API ] Slower path for RDG / external lookup.
 */
void DestroyMaterial(entt::registry& registry, ResourceID id);



} // namespace rrl::rhi 
