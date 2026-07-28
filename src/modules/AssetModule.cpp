// RRL/src/modules/AssetModule.cpp

#include "RRL/modules/AssetModule.hpp"
#include "RRL/EngineContext.hpp"

#include "RRL/asset/AssetManager.hpp"
#include "RRL/asset/PrefabBlueprintManager.hpp"
#include "RRL/EnttCasting.hpp"




namespace rrl {
    
// --- Constructor / Destructor ------------------------------------
AssetModule::AssetModule(EngineContext* ctx): m_ctx(ctx) {
    rrl::asset::InitializeAssetManager(m_ctx->registry);
    rrl::asset::InitializePrefabManager(m_ctx->registry); // For managing complete blueprints
}
AssetModule::~AssetModule() {
    // This automatically frees asset allocated RHI memory
    if (m_ctx != nullptr) {
        rrl::asset::DestroyAllAssets(m_ctx->registry);
    }
}




// --- Lifecycle ---------------------------------------------------
void AssetModule::SetAssetGCPolicy(rrl::asset::AssetGCPolicy policy) {
    rrl::asset::SetAssetGCPolicy(m_ctx->registry, policy);
}

rrl::asset::AssetGCPolicy AssetModule::GetAssetGCPolicy() const {
    return rrl::asset::GetAssetGCPolicy(m_ctx->registry);
}

void AssetModule::FreeUnusedAssets() {
    rrl::asset::FreeUnusedAssets(m_ctx->registry);
}

void AssetModule::DestroyAsset(AssetID asset) {
    rrl::asset::DestroyAsset(m_ctx->registry, ToEntt(asset));
}

void AssetModule::DestroyAllAssets() {
    rrl::asset::DestroyAllAssets(m_ctx->registry);
}


// --- Textures ----------------------------------------------------
AssetID AssetModule::GetCachedTexture(const rrl::asset::TextureID& texture_id) const {
    return ToAssetID( rrl::asset::GetCachedTexture(m_ctx->registry, texture_id) );
}

AssetID AssetModule::CreateTexture(const rrl::asset::TextureID& texture_id, rrl::asset::ImageAsset&& image_data) {
    // std::move is strictly required here to transfer ownership of the data buffer
    return ToAssetID( rrl::asset::CreateTexture(m_ctx->registry, texture_id, std::move(image_data)) );
}

void AssetModule::UpdateTexture(AssetID texture_asset, rrl::asset::ImageAsset&& image_data) {
    rrl::asset::UpdateTexture(m_ctx->registry, ToEntt(texture_asset), std::move(image_data));
}


// --- Meshes ------------------------------------------------------
AssetID AssetModule::GetCachedMesh(const rrl::asset::MeshID& mesh_id) const {
    return ToAssetID( rrl::asset::GetCachedMesh(m_ctx->registry, mesh_id) );
}

AssetID AssetModule::CreateMesh(const rrl::asset::MeshID& mesh_id, rrl::asset::MeshAsset&& mesh_data) {
    // std::move is strictly required here to transfer ownership of the geometry vectors
    return ToAssetID( rrl::asset::CreateMesh(m_ctx->registry, mesh_id, std::move(mesh_data)) );
}

void AssetModule::UpdateMesh(AssetID mesh_asset, rrl::asset::MeshAsset&& mesh_data) {
    rrl::asset::UpdateMesh(m_ctx->registry, ToEntt(mesh_asset), std::move(mesh_data));
}

void AssetModule::BindMesh(ObjectID world_object, AssetID mesh_asset, 
              const std::vector<AssetID>& materials, 
              rhi::RHIRenderLayerMask layer) {
    
    // We must unpack the strong-typed AssetIDs into a vector of raw entt::entities
    std::vector<entt::entity> internal_materials;
    internal_materials.reserve(materials.size());
    for (AssetID mat_id : materials) {
        internal_materials.push_back(ToEntt(mat_id));
    }

    rrl::asset::BindMesh(m_ctx->registry, ToEntt(world_object), ToEntt(mesh_asset), internal_materials, layer);
}

void AssetModule::SetMeshLayer(ObjectID world_object, rhi::RHIRenderLayerMask layer) {
    rrl::asset::SetMeshLayer(m_ctx->registry, ToEntt(world_object), layer);
}


// --- Materials ---------------------------------------------------
AssetID AssetModule::GetCachedMaterial(const rrl::asset::MaterialID& material_id) const {
    return ToAssetID( rrl::asset::GetCachedMaterial(m_ctx->registry, material_id) );
}

AssetID AssetModule::CreateMaterial(const rrl::asset::MaterialID& material_id, const rrl::asset::MaterialAsset& mat_data) {
    return ToAssetID( rrl::asset::CreateMaterial(m_ctx->registry, material_id, mat_data) );
}

void AssetModule::UpdateMaterial(AssetID material_asset, const rrl::asset::MaterialAsset& mat_data) {
    rrl::asset::UpdateMaterial(m_ctx->registry, ToEntt(material_asset), mat_data);
}

void AssetModule::BindMaterialTexture(AssetID material_asset, AssetID texture_asset, 
                 rrl::asset::MaterialTextureType texture_type) {
    rrl::asset::BindMaterialTexture(m_ctx->registry, ToEntt(material_asset), ToEntt(texture_asset), texture_type);
}

// --- Prefabs -----------------------------------------------------
void AssetModule::PreloadPrefabBlueprint(const rrl::asset::PrefabID& blueprint_id, rrl::io::IOPrefab&& prefab_data) {
    rrl::asset::PreloadPrefabBlueprint(m_ctx->registry, blueprint_id, std::move(prefab_data));
}
/*
// TODO:
void AssetModule::UnloadPrefabBlueprint(const rrl::asset::PrefabID& blueprint_id) {
    rrl::asset::UnloadPrefabBlueprint(m_ctx->registry, blueprint_id);
}
*/


// --- UI Assets ---------------------------------------------------
void AssetModule::BindUITexture(ObjectID ui_object, AssetID texture_asset,
                   float screen_x, float screen_y, 
                   float screen_w, float screen_h,
                   rhi::RHIRenderLayerMask layer) {
    rrl::asset::BindUITexture(m_ctx->registry, ToEntt(ui_object), ToEntt(texture_asset), screen_x, screen_y, screen_w, screen_h, layer);
}

void AssetModule::UpdateUILayout(ObjectID ui_object, 
                    float screen_x, float screen_y, 
                    float screen_w, float screen_h, 
                    rhi::RHIRenderLayerMask layer) {
    rrl::asset::UpdateUILayout(m_ctx->registry, ToEntt(ui_object), screen_x, screen_y, screen_w, screen_h, layer);
}


} // namespace rrl

