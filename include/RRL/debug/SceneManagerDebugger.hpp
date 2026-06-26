// RRL/include/debug/SceneManagerDebugger.hpp
#pragma once

#include "RRL/scene/SceneManager.hpp"
#include <entt/entt.hpp>


namespace rrl::debug::scene {


// --- Debug Structs -----------------------------------------------
// The blueprints have a tree structure we must mimic on the debug reports
// root: [node1, node2] | node1: [node1.1, node1.2] | node2: [] | node1.1: [] | node1.2: []
struct BlueprintNodeStats {
    std::string name;           // node name ('car', 'wheel'...)
    entt::entity mesh_asset;    // entt::null if it is just an empty transform node
    
    std::vector<BlueprintNodeStats> children;
};
struct BlueprintDebugStats {
    rrl::scene::BlueprintID blueprint_id;
    std::vector<BlueprintNodeStats> root_nodes;

    // How many live instances are using this blueprint?
    uint32_t live_instances_count {0}; 

};

struct SceneDebugReport {
    // Blueprints safely loaded in RAM
    std::unordered_map<rrl::scene::BlueprintID, BlueprintDebugStats> tracked_blueprints;
    
    // Entities in the world that have a PrefabInstanceComponent, but the Blueprint no longer exists in the cache
    std::vector<entt::entity> leaked_instances; 
};


// --- API ---------------------------------------------------------
/**
 * @brief Cross-references the registry against the PrefabCache to generate a full scene diagnostic report.
 */
SceneDebugReport GetSceneDebugReport(entt::registry& registry);

} // namespace rrl::debug::scene
