// RRL/src/scene/SceneManager.cpp

#include "RRL/scene/SceneManager.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/tf/TransformTree.hpp"


#include "RRL/DebugMacros.hpp"
#include <sstream>
#include <vector>


namespace rrl::scene {



// --- Cache Definitiosn -------------------------------------------
struct PrefabNodeBlueprint {
    std::string name; // this object (shape) specific name ('car', 'wheel'...)
    entt::entity mesh_asset { entt::null };
    
    // Node default relative transform to its parent
    glm::vec3 local_position {0.0f};
    glm::quat local_rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 local_scale {1.0f};
    
    // Node's children
    std::vector<PrefabNodeBlueprint> children;
};

struct PrefabBlueprint {
    BlueprintID id; // Prefab ID
    std::vector<PrefabNodeBlueprint> root_nodes;
};

struct PrefabCache {
    std::unordered_map<BlueprintID, PrefabBlueprint> blueprints;
};


// --- Helpers -----------------------------------------------------
static PrefabNodeBlueprint ProcessIONodeRecursive(entt::registry& registry, io::IONode&& io_node, const std::vector<entt::entity>& real_materials) {
    PrefabNodeBlueprint node_bp;
    node_bp.name = io_node.name;
    node_bp.local_position = io_node.local_position;
    node_bp.local_rotation = io_node.local_rotation;
    node_bp.local_scale = io_node.local_scale;

    if (!io_node.mesh.positions.empty()) {
        auto sub_meshes = io_node.mesh.materials;
        entt::entity mesh_asset = data::CreateMesh(registry, std::move(io_node.mesh));

        for (const auto& sub_mesh : sub_meshes) {
            int mat_idx = static_cast<int>(entt::to_integral(sub_mesh.material_entity));
            if (mat_idx >= 0 && mat_idx < real_materials.size()) {
                data::BindMaterial(registry, mesh_asset, real_materials[mat_idx], sub_mesh.index_offset, sub_mesh.index_count);
            }
        }
        node_bp.mesh_asset = mesh_asset;
        
        // This ensures the Mesh (and its cascaded materials/textures) stay alive 
        // as long as this blueprint exists in the cache!
        data::IncrementAssetRef(registry, mesh_asset);
    }

    for (auto& child_io : io_node.children) {
        node_bp.children.push_back(ProcessIONodeRecursive(registry, std::move(child_io), real_materials));
    }

    return node_bp;
}
static const PrefabNodeBlueprint* ResolveBlueprintPath(const PrefabCache& cache, const BlueprintID& node_id_path) {
    std::stringstream ss(node_id_path);
    std::string token;
    
    // Extract the root ID
    // Splits "city.car.wheel" into -> root: "city", path: ["car", "wheel"]
    std::getline(ss, token, '.');
    auto it = cache.blueprints.find(token);
    if (it == cache.blueprints.end()) return nullptr;

    const PrefabBlueprint& root_blueprint = it->second;
    const std::vector<PrefabNodeBlueprint>* current_level = &root_blueprint.root_nodes;
    const PrefabNodeBlueprint* target_node = nullptr;

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
static void SpawnNodeRecursive(entt::registry& registry, entt::entity parent_entity, const PrefabNodeBlueprint& node, const std::string& parent_path) {
    entt::entity child_entity = registry.create();
    
    // node id path = "city.car.wheel"
    std::string my_path = parent_path + "." + node.name;
    registry.emplace<PrefabInstanceComponent>(child_entity, my_path);
    if (node.mesh_asset != entt::null) {
        data::BindMesh(registry, child_entity, node.mesh_asset);
    }
    
    tf::AddTransform(registry, child_entity, parent_entity, node.local_position, node.local_rotation, node.local_scale, tf::TFDependencyPolicy::CASCADE_DELETE);

    for (const auto& child_node : node.children) {
        SpawnNodeRecursive(registry, child_entity, child_node, my_path);
    }
}
static void UnpinBlueprintAssetsRecursive(entt::registry& registry, const PrefabNodeBlueprint& node) {
    // Safely unpins all assets when a blueprint is being removed from RAM
    if (node.mesh_asset != entt::null) {
        data::DecrementAssetRef(registry, node.mesh_asset);
    }
    for (const auto& child : node.children) {
        UnpinBlueprintAssetsRecursive(registry, child);
    }
}


// --- Lifecycle ---------------------------------------------------
void InitializeSceneManager(entt::registry& registry) {
    registry.ctx().emplace<PrefabCache>();
}
void PreloadPrefabBlueprint(entt::registry& registry, const BlueprintID& blueprint_id, io::IOPrefab&& prefab_data) {
    RRL_ASSERT(registry.ctx().contains<PrefabCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<PrefabCache>();

    if (cache.blueprints.find(blueprint_id) != cache.blueprints.end()) {
        LOG_WARN("[SceneManager] Blueprint '{}' is already cached.", blueprint_id);
        return;
    }
    
    PrefabBlueprint blueprint;
    blueprint.id = blueprint_id;
    
    // Upload Materials and bind Textures
    std::vector<entt::entity> real_materials;
    for (auto& io_mat : prefab_data.materials) {
        entt::entity mat_entity = data::CreateMaterial(registry, io_mat.material_parameters);
        
        if (!io_mat.albedo_path.empty()) {
            entt::entity tex_entity = data::LoadTextureFromFile(registry, io_mat.albedo_path);
            data::BindMaterialTexture(registry, mat_entity, tex_entity, data::MaterialTextureType::ALBEDO);
        }
        if (!io_mat.normal_path.empty()) {
            entt::entity tex_entity = data::LoadTextureFromFile(registry, io_mat.normal_path);
            data::BindMaterialTexture(registry, mat_entity, tex_entity, data::MaterialTextureType::NORMAL_MAP);
        }
        real_materials.push_back(mat_entity);
    }

    // Recursively Upload Geometry and Map to Blueprint Tree
    for (auto& io_root_node : prefab_data.root_nodes) {
        blueprint.root_nodes.push_back(
            ProcessIONodeRecursive(registry, std::move(io_root_node), real_materials)
        );
    }

    cache.blueprints[blueprint_id] = std::move(blueprint);
}
entt::entity SpawnPrefab(entt::registry& registry, const BlueprintID& node_id_path) {
    RRL_ASSERT(registry.ctx().contains<PrefabCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<PrefabCache>();

    entt::entity root = registry.create();
    registry.emplace<PrefabInstanceComponent>(root, node_id_path);

    // Check if the path has dots (requesting a sub-node)
    if (node_id_path.find('.') != std::string::npos) {
        const PrefabNodeBlueprint* target_node = ResolveBlueprintPath(cache, node_id_path);
        if (!target_node) {
            LOG_ERROR("[SceneManager] Invalid blueprint node_id_path: '{}'", node_id_path);
            registry.destroy(root);
            return entt::null;
        }

        if (target_node->mesh_asset != entt::null) data::BindMesh(registry, root, target_node->mesh_asset);
        tf::AddTransform(registry, root, target_node->local_position, target_node->local_rotation, target_node->local_scale); 

        for (const auto& child_node : target_node->children) {
            SpawnNodeRecursive(registry, root, child_node, node_id_path);
        }
    } 
    else {
        // Flat root request (e.g., "city")
        if (cache.blueprints.find(node_id_path) == cache.blueprints.end()) {
            LOG_ERROR("[SceneManager] Root blueprint '{}' not found.", node_id_path);
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
void DestroyPrefab(entt::registry& registry, entt::entity prefab_entity, bool force_asset_deletion) {
    if (!registry.valid(prefab_entity)) return;

    if (force_asset_deletion) {
        if (auto* instance = registry.try_get<PrefabInstanceComponent>(prefab_entity)) {
            RRL_ASSERT(registry.ctx().contains<PrefabCache>(), "SceneManager not initialized!");
            auto& cache = registry.ctx().get<PrefabCache>();
            
            std::string root_id = instance->path.substr(0, instance->path.find('.'));
            
            // Unpin before erasing
            if (cache.blueprints.find(root_id) != cache.blueprints.end()) {
                for (const auto& root_node : cache.blueprints.at(root_id).root_nodes) {
                    UnpinBlueprintAssetsRecursive(registry, root_node);
                }
                cache.blueprints.erase(root_id);
            }
        }

        auto prev_policy = data::GetAssetGCPolicy(registry);
        data::SetAssetGCPolicy(registry, data::AssetGCPolicy::CASCADE_DELETE); 
        registry.destroy(prefab_entity);
        data::SetAssetGCPolicy(registry, prev_policy);
    } 
    else {
        registry.destroy(prefab_entity);
    }
}




} // namespace rrl::scene
