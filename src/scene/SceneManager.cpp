// RRL/src/scene/SceneManager.cpp

#include "RRL/scene/SceneManager.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/tf/TransformTree.hpp"


#include "RRL/DebugMacros.hpp"


namespace rrl::scene {



// --- Cache Definitiosn -------------------------------------------
struct PrefabNodeBlueprint {
    std::string name; // shape_name or internal blueprint node name
    entt::entity mesh_asset { entt::null };
};

struct PrefabBlueprint {
    BlueprintID id; // Prefab ID
    std::vector<PrefabNodeBlueprint> nodes;
};

struct PrefabCache {
    std::unordered_map<BlueprintID, PrefabBlueprint> blueprints;
};


// --- Lifecycle ---------------------------------------------------
void InitializeSceneManager(entt::registry& registry) {
    registry.ctx().emplace<PrefabCache>();
}

void PreloadPrefabBlueprint(entt::registry& registry, const std::string& blueprint_id, io::IOPrefab&& prefab_data) {
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
        
        // Load and bind textures using the API (Increments texture ref count)
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

    // Upload Geometry and bind Materials
    for (auto& io_shape : prefab_data.shapes) {
        
        // Copy the parsed material layout BEFORE we std::move the shape memory
        auto sub_meshes = io_shape.mesh.materials;
        entt::entity mesh_asset = data::CreateMesh(registry, std::move(io_shape.mesh));

        // Bind the materials to the geometry
        for (const auto& sub_mesh : sub_meshes) {
            int mat_idx = static_cast<int>(entt::to_integral(sub_mesh.material_entity));
            if (mat_idx >= 0 && mat_idx < real_materials.size()) {
                data::BindMaterial(
                    registry, 
                    mesh_asset, 
                    real_materials[mat_idx], 
                    sub_mesh.index_offset, 
                    sub_mesh.index_count
                );
            }
        }

        blueprint.nodes.push_back({ io_shape.name, mesh_asset });
    }

    cache.blueprints[blueprint_id] = std::move(blueprint);
}

entt::entity SpawnPrefab(entt::registry& registry, const std::string& blueprint_id) {
    RRL_ASSERT(registry.ctx().contains<PrefabCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<PrefabCache>();

    if (cache.blueprints.find(blueprint_id) == cache.blueprints.end()) {
        LOG_ERROR("[SceneManager] Cannot spawn. Blueprint '{}' not found in cache.", blueprint_id);
        return entt::null;
    }

    const PrefabBlueprint& blueprint = cache.blueprints.at(blueprint_id);

    // Instantiate Root Entity
    entt::entity root = registry.create();
    registry.emplace<PrefabInstanceComponent>(root, blueprint_id);
    tf::AddTransform(registry, root); // Default World Transform

    // Instantiate Child Nodes
    for (const auto& node : blueprint.nodes) {
        entt::entity child = registry.create();
        
        // Bind the VRAM memory handle
        data::BindMesh(registry, child, node.mesh_asset);
        
        // Attach to the root hierarchy
        tf::AddTransform(
            registry, 
            child, 
            root, 
            glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f),
            tf::TFDependencyPolicy::CASCADE_DELETE
        );
    }

    return root;
}
void DestroyPrefab(entt::registry& registry, entt::entity prefab_entity, bool force_asset_deletion) {
    if (!registry.valid(prefab_entity)) return;

    if (force_asset_deletion) {
        if (auto* instance = registry.try_get<PrefabInstanceComponent>(prefab_entity)) {
            RRL_ASSERT(registry.ctx().contains<PrefabCache>(), "SceneManager not initialized!");
            auto& cache = registry.ctx().get<PrefabCache>();
            cache.blueprints.erase(instance->blueprint_id);
        }

        // FORCE the cascade so VRAM assets are destroyed when the ref count hits 0
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
