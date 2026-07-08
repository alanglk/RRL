// RRL/src/debug/SceneManagerDebugger.hpp

#include "RRL/debug/SceneManagerDebugger.hpp"
#include "RRL/scene/SceneCache.hpp"

#include "RRL/DebugMacros.hpp"
#include "RRL/scene/SceneManager.hpp"

namespace rrl::debug::scene {

// --- Helpers -----------------------------------------------------
// Recursive helper to map the internal blueprint tree to the debug tree
static BlueprintNodeStats MapNodeToStats(const rrl::scene::PrefabNodeBlueprint& internal_node) {
    BlueprintNodeStats stats;
    stats.name = internal_node.name;
    stats.mesh_asset = internal_node.mesh_asset;

    for (const auto& child : internal_node.children) {
        stats.children.push_back(MapNodeToStats(child));
    }
    
    return stats;
}


// --- API ---------------------------------------------------------
SceneDebugReport GetSceneDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::scene::PrefabCache>(), "SceneManager not initialized!");
    SceneDebugReport report;

    // Safety check: Is the SceneManager initialized?
    if (!registry.ctx().contains<rrl::scene::PrefabCache>()) {
        return report;
    }

    const auto& cache = registry.ctx().get<rrl::scene::PrefabCache>().blueprints;

    // Tracked blueprints
    for (const auto& [id, blueprint] : cache) {
        BlueprintDebugStats stats;
        stats.blueprint_id = id;
        stats.live_instances_count = 0;

        for (const auto& root_node : blueprint.root_nodes) {
            stats.root_nodes.push_back(MapNodeToStats(root_node));
        }

        report.tracked_blueprints[id] = stats;
    }

    // Cross-reference with live entities in the world
    auto view = registry.view<rrl::scene::PrefabInstanceComponent>();
    for (auto entity : view) {
        const auto& instance = view.get<rrl::scene::PrefabInstanceComponent>(entity);
        
        // Extract the root blueprint ID (e.g., "city.car.wheel" -> "city")
        rrl::scene::BlueprintID root_id = instance.path.substr(0, instance.path.find('.'));

        // Does this blueprint still exist in the cache?
        auto it = report.tracked_blueprints.find(root_id);
        if (it != report.tracked_blueprints.end()) {
            it->second.live_instances_count++;
        } else {
            // The entity exists, but the template it was spawned from has been deleted!
            report.leaked_instances.push_back(entity);
        }
    }

    return report;
}


} // namespace rrl::debug::scene

