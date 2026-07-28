// RRL/src/include/RRL/scene/SceneManager.hpp
#pragma once

#include "RRL/asset/AssetTypes.hpp"

#include <entt/entt.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <vector>

namespace rrl::asset {


// --- Cache Definitiosn -------------------------------------------
struct PrefabNodeBlueprint {
    std::string name; // this object (shape) specific name ('car', 'wheel'...)
    entt::entity mesh_asset { entt::null };
    
    // Node default relative transform to its parent
    glm::vec3 local_position {0.0f};
    glm::quat local_rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 local_scale {1.0f};
    
    // Node's linked materials
    std::vector<entt::entity> materials;
    
    // Node's children
    std::vector<PrefabNodeBlueprint> children;
};

struct PrefabBlueprint {
    PrefabID id; // Prefab ID
    std::vector<PrefabNodeBlueprint> root_nodes;
};

struct PrefabCache {
    std::unordered_map<PrefabID, PrefabBlueprint> blueprints;
};


} // namespace rrl::scene
