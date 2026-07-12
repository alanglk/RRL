// RRL/include/debug/SceneDebugTypes.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/RRLTypes.hpp"
#include "RRL/scene/SceneTypes.hpp"
#include <unordered_map>
#include <vector>


namespace rrl::debug {



// --- Prefab Debug Structs ----------------------------------------
// The prefab blueprints have a tree structure we must mimic on the debug reports
// root: [node1, node2] | node1: [node1.1, node1.2] | node2: [] | node1.1: [] | node1.2: []
struct RRL_API BlueprintNodeStats {
    std::string name;           // node name ('car', 'wheel'...)
    AssetID mesh_asset;         // NULL_ASSET if it is just an empty transform node
    
    std::vector<BlueprintNodeStats> children;
};
struct RRL_API BlueprintDebugStats {
    rrl::scene::PrefabID blueprint_id;
    std::vector<BlueprintNodeStats> root_nodes;

    // How many live instances are using this blueprint?
    uint32_t live_instances_count {0}; 

};



// --- Debug Structs -----------------------------------------------
struct RRL_API SceneDebugReport {
    // Blueprints safely loaded in RAM
    std::unordered_map<rrl::scene::PrefabID, BlueprintDebugStats> tracked_blueprints;
    
    // Entities in the world that have a PrefabInstanceComponent, but the Blueprint no longer exists in the cache
    std::vector<AssetID> leaked_instances; 
};


} // namespace rrl::debug

