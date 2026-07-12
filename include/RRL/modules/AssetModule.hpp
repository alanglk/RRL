// RRL/include/modules/AssetModule.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/asset/ImageAsset.hpp"
#include "RRL/asset/MaterialAsset.hpp"
#include "RRL/asset/MeshAsset.hpp"

#include "RRL/asset/AssetTypes.hpp"
#include "RRL/rhi/RHITypes.hpp"
#include "RRL/RRLTypes.hpp"


namespace rrl {
    

//  RRL::Engine runtime context.
struct EngineContext;


class RRL_API AssetModule {
public:
    // --- Constructor / Destructor ------------------------------------
    explicit AssetModule(EngineContext* ctx);
    ~AssetModule();

    // Rule of five (this class is owned by RRLEngine)
    AssetModule(const AssetModule&)             = delete;
    AssetModule& operator=(const AssetModule&)  = delete;
    AssetModule(AssetModule&&)                  = delete;
    AssetModule& operator=(AssetModule&&)       = delete;


    // --- Lifecycle ---------------------------------------------------
    /**
     * @brief Sets the global garbage collection policy for the asset manager.
     */
    void SetAssetGCPolicy(rrl::asset::AssetGCPolicy policy);
    /**
     * @brief Gets the global garbage collection policy of the asset manager.
     */
    rrl::asset::AssetGCPolicy GetAssetGCPolicy() const;
    /**
     * @brief Free all assets with 0 active references.
     * Highly recommended to call this during scene transitions if KEEP_CACHED_ASSETS is active.
     */
    void FreeUnusedAssets();
    /**
     * @brief Safely destroys an asset (Texture, Mesh, Material).
     * Automatically frees the associated RHI backend memory via ECS hooks.
     */
    void DestroyAsset(AssetID asset);
    /**
     * @brief Clears all cached assets.
     */
    void DestroyAllAssets();



    // --- Textures ----------------------------------------------------
    /**
     * @brief Checks if a texture already exists in the cache using its unique ID (filepath or virtual name).
     * Returns NULL_ASSET if it doesn't exist.
     */
    AssetID GetCachedTexture(const rrl::asset::TextureID& texture_id) const;
    /**
     * @brief Creates a texture (handles RHI allocation and uploading).
     * Takes ownership of the data via move semantics to prevent deep copies.
     * If the texture_id already exists, it gets overrided by a new texture.
     */
    AssetID CreateTexture(const rrl::asset::TextureID& texture_id, rrl::asset::ImageAsset&& image_data);
    /**
     * @brief Updates a texture with new image data. (using move semantics).
     */
    void UpdateTexture(AssetID texture_asset, rrl::asset::ImageAsset&& image_data);



    // --- Meshes ------------------------------------------------------
    /**
     * @brief Checks if a mesh already exists in the cache.
     */
    AssetID GetCachedMesh(const rrl::asset::MeshID& mesh_id) const;
    /**
     * @brief Creates a mesh (handles RHI allocation and uploading).
     * Takes ownership of the data via move semantics to prevent deep copies.
     * If the mesh_id already exists, it gets overrided by a new mesh.
     */
    AssetID CreateMesh(const rrl::asset::MeshID& mesh_id, rrl::asset::MeshAsset&& mesh_data);
    /**
     * @brief Updates an existing mesh asset with new data.
     * Crucial for dynamic geometry like LIDAR point clouds or procedural terrain.
     */
    void UpdateMesh(AssetID mesh_asset, rrl::asset::MeshAsset&& mesh_data);
    /**
     * @brief Binds a loaded mesh asset to a physical world entity.
     * Maps the provided materials 1:1 to the mesh submeshes. 
     * If 1 material is provided, it is broadcast to all submeshes.
     */
    void BindMesh(ObjectID world_object, AssetID mesh_asset, 
                  const std::vector<AssetID>& materials = {}, 
                  rhi::RHIRenderLayer layer = rhi::RHIRenderLayer::LAYER_DEFAULT);
    /**
     * @brief Dynamically updates the culling layer of an already bound physical object.
     */
    void SetMeshLayer(ObjectID world_object, rhi::RHIRenderLayer layer);



    // --- Materials ---------------------------------------------------
    /**
     * @brief Gets a material that already exists in the cache using its unique ID.
     */
    AssetID GetCachedMaterial(const rrl::asset::MaterialID& material_id) const;
    /**
     * @brief Spawns a new Material entity and caches it under a specific ID.
     */
    AssetID CreateMaterial(const rrl::asset::MaterialID& material_id, const rrl::asset::MaterialAsset& mat_data);
    /**
     * @brief Updates an existing material with new parameters (e.g., changing colors dynamically).
     * Automatically syncs the changes to the underlying RHI hardware handle.
     */
    void UpdateMaterial(AssetID material_asset, const rrl::asset::MaterialAsset& mat_data);
    /**
     * @brief Binds a texture asset as a certain material texture type to an actual material asset.
     */
    void BindMaterialTexture(AssetID material_asset, AssetID texture_asset, 
                     rrl::asset::MaterialTextureType texture_type = rrl::asset::MaterialTextureType::ALBEDO);


    // --- UI Assets ---------------------------------------------------
    /**
     * @brief Binds a texture asset directly to a 2D UI / Screen entity.
     */
    void BindUITexture(ObjectID ui_object, AssetID texture_asset,
                       float screen_x = 0.0f, float screen_y = 0.0f, 
                       float screen_w = 1.0f, float screen_h = 1.0f,
                       rhi::RHIRenderLayer layer = rhi::RHIRenderLayer::LAYER_UI);

    /**
     * @brief Updates the screen dimensions of an existing 2D UI layout.
     * Asserts if the entity does not have a UI texture bound to it.
     */
    void UpdateUILayout(ObjectID ui_object, 
                        float screen_x, float screen_y, 
                        float screen_w, float screen_h, 
                        rhi::RHIRenderLayer layer = rhi::RHIRenderLayer::LAYER_UI);

private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl
