// RRL/include/data/AssetManager.hpp
#pragma once

#include "RRL/data/ImageData.hpp"
#include "RRL/data/MaterialData.hpp"
#include "RRL/data/MeshData.hpp"
#include "entt/entity/fwd.hpp"
#include <cstdint>
#include <entt/entt.hpp>


namespace rrl::data {


/**
 * @brief Defines how the AssetManager handles assets whose reference count drops to 0.
 */
enum class AssetGCPolicy : uint8_t {
    CASCADE_DELETE = 0,     // Destroy the asset and cascade the deletion
    KEEP_CACHED_ASSETS = 0  // Keep the asset in memory for future usage
};


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
 * @brief Loads a texture from disk into the ECS. 
 * Returns the cached entity ID if already loaded.
 */
entt::entity LoadTextureFromFile(entt::registry& registry, const std::string& filepath);
/**
 * @brief Creates a texture from an existing ImageData struct.
 * Takes ownership of the data via move semantics to prevent deep copies.
 */
entt::entity CreateTexture(entt::registry& registry, ImageData&& image_data);
/**
 * @brief Creates an empty dynamic texture buffer (e.g., for video streaming).
 */
entt::entity CreateTexture(entt::registry& registry, uint32_t width, uint32_t height, 
                           ImageChannelLayout channels, ImageDataType type = ImageDataType::UINT8);
/**
 * @brief Updates a texture with new image data. (using move semantics).
 */
void UpdateTexture(entt::registry& registry, entt::entity texture_asset, ImageData&& image_data);


// --- Meshes ------------------------------------------------------
/**
 * @brief Loads a 3D mesh from disk into the ECS. 
 * Returns the cached entity ID if already loaded.
 */
entt::entity LoadMeshFromFile(entt::registry& registry, const std::string& filepath);
/**
 * @brief Creates a mesh from an existing MeshData struct.
 * Takes ownership of the data via move semantics to prevent deep copies.
 */
entt::entity CreateMesh(entt::registry& registry, MeshData&& mesh_data);
/**
 * @brief Creates an empty dynamic mesh buffer.
 */
entt::entity CreateMesh(entt::registry& registry, MeshTopology topology = MeshTopology::TRIANGLES);
/**
 * @brief Binds a loaded mesh asset to a physical world entity.
 * This tells the RHI to draw the mesh at the entity's Transform location.
 */
void BindMesh(entt::registry& registry, entt::entity world_object, entt::entity mesh_asset);



// --- Materials ---------------------------------------------------
/**
 * @brief Spawns a new Material entity in the ECS.
 */
entt::entity CreateMaterial(entt::registry& registry, const MaterialData& mat_data);
/**
 * @brief A helper to quickly apply a material to a mesh asset.
 * If index_count is 0, it applies the material to the entire mesh.
 */
void BindMaterial(entt::registry& registry, entt::entity mesh_asset, entt::entity material_asset, 
                  uint32_t index_offset = 0, uint32_t index_count = 0);
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
                   float screen_w = 1.0f, float screen_h = 1.0f);
/**
 * @brief Updates the screen dimensions of an existing 2D UI layout.
 * Asserts if the entity does not have a UI texture bound to it.
 */
void UpdateUILayout(entt::registry& registry, entt::entity ui_object, float screen_x, float screen_y, float screen_w, float screen_h);



} // namespace rrl::data