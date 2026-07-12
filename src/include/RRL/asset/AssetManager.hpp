// RRL/src/include/data/AssetManager.hpp
#pragma once


#include "RRL/asset/AssetTypes.hpp"

#include "RRL/asset/ImageAsset.hpp"
#include "RRL/asset/MaterialAsset.hpp"
#include "RRL/asset/MeshAsset.hpp"


#include "RRL/rhi/RHITypes.hpp"


#include <entt/entt.hpp>


namespace rrl::asset {

// --- Lifecycle ---------------------------------------------------
/**
 * @brief Initializes the asset system and registers EnTT memory cleanup hooks.
 * The default AssetGCPolicy is set to CASCADE_DELETE.
 */
void InitializeAssetManager(entt::registry& registry);
/**
 * @brief Sets the global garbage collection policy for the asset manager.
 */
void SetAssetGCPolicy(entt::registry& registry, AssetGCPolicy policy);
/**
 * @brief Gets the global garbage collection policy of the asset manager.
 */
AssetGCPolicy GetAssetGCPolicy(entt::registry& registry);
/**
 * @brief Free all assets with 0 active references.
 * Highly recommended to call this during scene transitions if KEEP_CACHED_ASSETS is active.
 */
void FreeUnusedAssets(entt::registry& registry);
/**
 * @brief Safely destroys an asset (Texture, Mesh, Material).
 * Automatically frees the associated RHI backend memory via ECS hooks.
 */
void DestroyAsset(entt::registry& registry, entt::entity asset_entity);
/**
 * @brief Clears all cached assets.
 */
void DestroyAllAssets(entt::registry& registry);



// --- Textures ----------------------------------------------------
/**
 * @brief Checks if a texture already exists in the cache using its unique ID (filepath or virtual name).
 * Returns entt::null if it doesn't exist.
 */
entt::entity GetCachedTexture(entt::registry& registry, const TextureID& texture_id);
/**
 * @brief Creates a texture (handles RHI allocation and uploading).
 * Takes ownership of the data via move semantics to prevent deep copies.
 * If the texture_id already exists, it gets overrided by a new texture.
 */
entt::entity CreateTexture(entt::registry& registry, const TextureID& texture_id, ImageAsset&& image_data);
/**
 * @brief Updates a texture with new image data. (using move semantics).
 */
void UpdateTexture(entt::registry& registry, entt::entity texture_asset, ImageAsset&& image_data);



// --- Meshes ------------------------------------------------------
/**
 * @brief Checks if a mesh already exists in the cache.
 */
entt::entity GetCachedMesh(entt::registry& registry, const MeshID& mesh_id);
/**
 * @brief Creates a mesh (handles RHI allocation and uploading).
 * Takes ownership of the data via move semantics to prevent deep copies.
 * If the mesh_id already exists, it gets overrided by a new mesh.
 */
entt::entity CreateMesh(entt::registry& registry, const MeshID& mesh_id,MeshAsset&& mesh_data);
/**
 * @brief Updates an existing mesh asset with new data.
 * Crucial for dynamic geometry like LIDAR point clouds or procedural terrain.
 */
void UpdateMesh(entt::registry& registry, entt::entity mesh_asset, MeshAsset&& mesh_data);
/**
 * @brief Binds a loaded mesh asset to a physical world entity.
 * Maps the provided materials 1:1 to the mesh submeshes. 
 * If 1 material is provided, it is broadcast to all submeshes.
 */
void BindMesh(entt::registry& registry, entt::entity world_object, entt::entity mesh_asset, 
              const std::vector<entt::entity>& materials = {}, 
              rhi::RHIRenderLayer layer = rhi::RHIRenderLayer::LAYER_DEFAULT);
/**
 * @brief Dynamically updates the culling layer of an already bound physical object.
 */
void SetMeshLayer(entt::registry& registry, entt::entity world_object, rhi::RHIRenderLayer layer);



// --- Materials ---------------------------------------------------
/**
 * @brief Gets a material that already exists in the cache using its unique ID.
 */
entt::entity GetCachedMaterial(entt::registry& registry, const MaterialID& material_id);
/**
 * @brief Spawns a new Material entity and caches it under a specific ID.
 */
entt::entity CreateMaterial(entt::registry& registry, const MaterialID& material_id, const MaterialAsset& mat_data);
/**
 * @brief Updates an existing material with new parameters (e.g., changing colors dynamically).
 * Automatically syncs the changes to the underlying RHI hardware handle.
 */
void UpdateMaterial(entt::registry& registry, entt::entity material_asset, const MaterialAsset& mat_data);
/**
 * @brief Binds a texture asset as a certain material texture type to an actual material asset.
 */
void BindMaterialTexture(entt::registry& registry, entt::entity material_asset, entt::entity texture_asset, 
                 MaterialTextureType texture_type = MaterialTextureType::ALBEDO);


// --- UI Assets ---------------------------------------------------
/**
 * @brief Binds a texture asset directly to a 2D UI / Screen entity.
 */
void BindUITexture(entt::registry& registry, entt::entity ui_object, entt::entity texture_asset,
                   float screen_x = 0.0f, float screen_y = 0.0f, 
                   float screen_w = 1.0f, float screen_h = 1.0f,
                   rhi::RHIRenderLayer layer = rhi::RHIRenderLayer::LAYER_UI);

/**
 * @brief Updates the screen dimensions of an existing 2D UI layout.
 * Asserts if the entity does not have a UI texture bound to it.
 */
void UpdateUILayout(entt::registry& registry, entt::entity ui_object, 
                    float screen_x, float screen_y, 
                    float screen_w, float screen_h, 
                    rhi::RHIRenderLayer layer = rhi::RHIRenderLayer::LAYER_UI);


// --- GB Interface ------------------------------------------------
/**
 * @brief Manually increments an asset's reference counter to keep it alive.
 */
void IncrementAssetRef(entt::registry& registry, entt::entity asset);

/**
 * @brief Manually decrements an asset's reference counter. 
 * If it hits 0 and the policy is CASCADE_DELETE, it is destroyed.
 */
void DecrementAssetRef(entt::registry& registry, entt::entity asset);


} // namespace rrl::asset