// RRL/src/scene/PrefabInstanceManager.cpp

#include "RRL/scene/PrefabInstanceManager.hpp"

#include "RRL/asset/PrefabCacheNodes.hpp"
#include "RRL/scene/PrefabInstance.hpp"

#include "RRL/asset/AssetManager.hpp"
#include "RRL/tf/TransformTree.hpp"


#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"

#include <sstream>
#include <vector>



namespace rrl::scene {


// --- Helpers -----------------------------------------------------
static const rrl::asset::PrefabNodeBlueprint* ResolveBlueprintPath(const rrl::asset::PrefabCache& cache, const rrl::asset::PrefabID& node_id_path) {
    std::stringstream ss(node_id_path);
    std::string token;
    
    // Extract the root ID
    // Splits "city.car.wheel" into -> root: "city", path: ["car", "wheel"]
    std::getline(ss, token, '.');
    auto it = cache.blueprints.find(token);
    if (it == cache.blueprints.end()) return nullptr;

    const rrl::asset::PrefabBlueprint& root_blueprint = it->second;
    const std::vector<rrl::asset::PrefabNodeBlueprint>* current_level = &root_blueprint.root_nodes;
    const rrl::asset::PrefabNodeBlueprint* target_node = nullptr;

    // Traverse the path
    while (std::getline(ss, token, '.')) {
        target_node = nullptr;
        for (const auto& node : *current_level) {
            if (node.name == token) {
                target_node = &node;
                current_level = &node.children;
                break;
            }
        }
        if (!target_node) return nullptr; 
    }

    // Return nullptr as "Root Request".
    return target_node; 
}
static void SpawnNodeRecursive(entt::registry& registry, entt::entity parent_entity, const rrl::asset::PrefabNodeBlueprint& node, const std::string& parent_path) {
    entt::entity child_entity = registry.create();
    std::string my_path = parent_path + "." + node.name;
    registry.emplace<rrl::scene::PrefabInstanceComponent>(child_entity, my_path);
    
    if (node.mesh_asset != entt::null) {
        // Pass the cached materials array cleanly into the Linkage!
        rrl::asset::BindMesh(registry, child_entity, node.mesh_asset, node.materials);
    }
    
    tf::AddTransform(registry, child_entity, parent_entity, node.local_position, node.local_rotation, node.local_scale, tf::TFDependencyPolicy::CASCADE_DELETE);
    for (const auto& child_node : node.children) SpawnNodeRecursive(registry, child_entity, child_node, my_path);
}
static void UnpinBlueprintAssetsRecursive(entt::registry& registry, const rrl::asset::PrefabNodeBlueprint& node) {
    // Safely unpins all assets when a blueprint is being removed from RAM
    if (node.mesh_asset != entt::null) {
        rrl::asset::DecrementAssetRef(registry, node.mesh_asset);
    }
    for (const auto& child : node.children) {
        UnpinBlueprintAssetsRecursive(registry, child);
    }
}



entt::entity SpawnPrefabInstance(entt::registry& registry, const rrl::asset::PrefabID& node_id_path) {
    RRL_ASSERT(registry.ctx().contains<rrl::asset::PrefabCache>(), "PrefabManager not initialized!");
    auto& cache = registry.ctx().get<rrl::asset::PrefabCache>();

    entt::entity root = registry.create();
    registry.emplace<rrl::scene::PrefabInstanceComponent>(root, node_id_path);

    // Check if the path has dots (requesting a sub-node)
    if (node_id_path.find('.') != std::string::npos) {
        const rrl::asset::PrefabNodeBlueprint* target_node = ResolveBlueprintPath(cache, node_id_path);
        if (!target_node) {
            LOG_ERROR("[PrefabManager] Invalid blueprint node_id_path: '{}'", node_id_path);
            registry.destroy(root);
            return entt::null;
        }

        if (target_node->mesh_asset != entt::null) rrl::asset::BindMesh(registry, root, target_node->mesh_asset);
        tf::AddTransform(registry, root, target_node->local_position, target_node->local_rotation, target_node->local_scale); 

        for (const auto& child_node : target_node->children) {
            SpawnNodeRecursive(registry, root, child_node, node_id_path);
        }
    } 
    else {
        // Flat root request (e.g., "city")
        if (cache.blueprints.find(node_id_path) == cache.blueprints.end()) {
            LOG_ERROR("[PrefabManager] Root blueprint '{}' not found.", node_id_path);
            registry.destroy(root);
            return entt::null;
        }
        
        tf::AddTransform(registry, root);
        for (const auto& root_node : cache.blueprints.at(node_id_path).root_nodes) {
            SpawnNodeRecursive(registry, root, root_node, node_id_path);
        }
    }

    return root;
}
void DestroyPrefabInstance(entt::registry& registry, entt::entity prefab_entity, bool force_asset_deletion) {
    if (!registry.valid(prefab_entity)) return;

    if (force_asset_deletion) {
        if (auto* instance = registry.try_get<rrl::scene::PrefabInstanceComponent>(prefab_entity)) {
            RRL_ASSERT(registry.ctx().contains<rrl::asset::PrefabCache>(), "PrefabManager not initialized!");
            auto& cache = registry.ctx().get<rrl::asset::PrefabCache>();
            
            std::string root_id = instance->path.substr(0, instance->path.find('.'));
            
            // Unpin before erasing
            if (cache.blueprints.find(root_id) != cache.blueprints.end()) {
                for (const auto& root_node : cache.blueprints.at(root_id).root_nodes) {
                    UnpinBlueprintAssetsRecursive(registry, root_node);
                }
                cache.blueprints.erase(root_id);
            }
        }

        auto prev_policy = rrl::asset::GetAssetGCPolicy(registry);
        rrl::asset::SetAssetGCPolicy(registry, rrl::asset::AssetGCPolicy::CASCADE_DELETE); 
        registry.destroy(prefab_entity);
        rrl::asset::SetAssetGCPolicy(registry, prev_policy);
    } 
    else {
        registry.destroy(prefab_entity);
    }
}




} // namespace rrl::scene
