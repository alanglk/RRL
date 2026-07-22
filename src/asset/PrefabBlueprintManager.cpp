// src/asset/PrefabBlueprintManager.cpp
#pragma once

#include "RRL/asset/PrefabBlueprintManager.hpp"
#include "RRL/asset/PrefabCacheNodes.hpp"

#include "RRL/asset/AssetManager.hpp"

#include "RRL/EnttCasting.hpp"
#include "RRL/io/ImageIO.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"



namespace rrl::asset {


// --- Helpers -----------------------------------------------------
static rrl::asset::PrefabNodeBlueprint ProcessIONodeRecursive(
    entt::registry& registry, 
    const rrl::asset::PrefabID& blueprint_id, 
    io::IONode&& io_node, 
    const std::unordered_map<std::string, entt::entity>& material_name_lut ) 
{
    rrl::asset::PrefabNodeBlueprint node_bp;
    node_bp.name = io_node.name;
    node_bp.local_position = io_node.local_position;
    node_bp.local_rotation = io_node.local_rotation;
    node_bp.local_scale = io_node.local_scale;

    if (!io_node.mesh.positions.empty()) {
        std::string mesh_id = blueprint_id + "_mesh_" + node_bp.name;
        entt::entity mesh_asset = rrl::asset::CreateMesh(registry, mesh_id, std::move(io_node.mesh));
        node_bp.mesh_asset = mesh_asset;
        
        for (const std::string& mat_name : io_node.submesh_material_names) {
            entt::entity matched_material = entt::null;
            auto it = material_name_lut.find(mat_name);
            if (it != material_name_lut.end()) matched_material = it->second;
            node_bp.materials.push_back(matched_material);
        }

        // This ensures the Mesh (and its cascaded materials/textures) stay alive 
        // as long as this blueprint exists in the cache!
        rrl::asset::IncrementAssetRef(registry, mesh_asset);
    }

    for (auto& child_io : io_node.children) {
        node_bp.children.push_back(ProcessIONodeRecursive(registry, blueprint_id, std::move(child_io), material_name_lut));
    }

    return node_bp;
}
static AssetID ResolveAndCacheTexture(entt::registry& registry, const std::string& filepath) {
    // Helper to load Images from disk if not cached on the asset manager
    if (filepath.empty()) return entt::null;
    entt::entity cached_tex = rrl::asset::GetCachedTexture(registry, filepath);
    if (cached_tex != entt::null) return ToAssetID( cached_tex );

    // Load disk payload inside PrefabManager I/O context
    rrl::asset::ImageAsset raw_image = rrl::io::LoadImage(filepath);
    if (raw_image.GetDataSize() == 0) {
        LOG_ERROR("[PrefabManager] Failed to load texture file: '{}'", filepath);
        return entt::null;
    }
    return ToAssetID( rrl::asset::CreateTexture(registry, filepath, std::move(raw_image)) );
}





// --- Lifecycle ---------------------------------------------------
void InitializePrefabManager(entt::registry& registry) {
    registry.ctx().emplace<rrl::asset::PrefabCache>();
}
void PreloadPrefabBlueprint(entt::registry& registry, const rrl::asset::PrefabID& blueprint_id, rrl::io::IOPrefab&& prefab_data) {
    RRL_ASSERT(registry.ctx().contains<rrl::asset::PrefabCache>(), "PrefabManager not initialized!");
    auto& cache = registry.ctx().get<rrl::asset::PrefabCache>();

    if (cache.blueprints.find(blueprint_id) != cache.blueprints.end()) {
        LOG_WARN("[PrefabManager] Blueprint '{}' is already cached.", blueprint_id);
        return;
    }
    
    rrl::asset::PrefabBlueprint blueprint;
    blueprint.id = blueprint_id;
    
    // Upload Materials and bind Textures
    std::unordered_map<std::string, entt::entity> material_name_lut;
    for (auto& io_mat : prefab_data.materials) {

        // Build a MaterialAsset payload 
        rrl::asset::MaterialAsset mat_payload = io_mat.material_parameters;

        // Resolve texture assets using our secure I/O decoupling helper
        mat_payload.albedo_map              = ResolveAndCacheTexture(registry, io_mat.albedo_path);
        mat_payload.normal_map              = ResolveAndCacheTexture(registry, io_mat.normal_path);
        mat_payload.metallic_roughness_map  = ResolveAndCacheTexture(registry, io_mat.metallic_roughness_path);
        mat_payload.emissive_map            = ResolveAndCacheTexture(registry, io_mat.emissive_path);

        // Generate a unique cache identifier for this material
        std::string material_cache_id = blueprint_id + "_mat_" + io_mat.name;
        entt::entity mat_entity = rrl::asset::CreateMaterial(registry, material_cache_id, mat_payload);
        material_name_lut[io_mat.name] = mat_entity;
    }

    // Recursively Upload Geometry and Map to Blueprint Tree
    for (auto& io_root_node : prefab_data.root_nodes) {
        blueprint.root_nodes.push_back(
            ProcessIONodeRecursive(registry, blueprint_id, std::move(io_root_node), material_name_lut)
        );
    }

    cache.blueprints[blueprint_id] = std::move(blueprint);
}
/*
// TODO:
void UnloadPrefabBlueprint(entt::registry& registry, const rrl::asset::PrefabID& blueprint_id) {
}
*/


} // namespace rrl::asset
