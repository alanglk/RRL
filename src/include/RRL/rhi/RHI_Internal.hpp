// RRL/src/include/rhi/RHI_Internal.hpp
#pragma once

#include <entt/entt.hpp>
#include "RRL/rhi/RHI.hpp"
#include "RRL/rhi/RHIBackend.hpp"



namespace rrl::rhi {


// --- Lifecycle ---------------------------------------------------
/**
 * @brief Synchronizes all modified CPU data components (Textures, Meshes, Materials) 
 * with their respective GPU/RHI hardware handles.
 * Should be called once per frame, right before rhi::RenderFrame.
 */
void SyncResources(entt::registry& registry);
/**
 * @brief Asks the backend to swap its TARGET_MAIN buffer to the attached window.
 */
void Present(entt::registry& registry);



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


} // namespace rrl::rhi 
