// RRL/src/include/scene/SceneCache.hpp
#pragma once

#include "RRL/scene/SceneManager.hpp"


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


} // namespace rrl::scene