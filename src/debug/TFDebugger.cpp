// RRL/src/debug/TFDebugger.cpp

#include "RRL/debug/TFDebugger.hpp"
#include "RRL/RRLTypes.hpp"
#include "RRL/tf/TFComponents.hpp"

#include "RRL/EnttCasting.hpp"


namespace rrl::debug::tf {

// --- Helpers -----------------------------------------------------
// Recursive helper to traverse the ECS pointers and build the tree
static TFNodeDebugStats BuildNodeStatsRecursive(
    entt::registry& registry, 
    entt::entity current_entity, 
    std::unordered_set<entt::entity>& visited,
    std::vector<ObjectID>& cyclical_nodes) 
{
    TFNodeDebugStats stats;
    stats.object_id = ToObjectID(current_entity);

    // If we have already visited the node -> cyclical tree
    if (visited.count(current_entity)) {
        cyclical_nodes.push_back( ToObjectID(current_entity) );
        return stats; 
    }
    visited.insert(current_entity);

    // Fetch the internal TF components
    const auto& rel   = registry.get<rrl::tf::TFRelationshipComponent>(current_entity);
    const auto& local = registry.get<rrl::tf::TFLocalTransformComponent>(current_entity);
    const auto& world = registry.get<rrl::tf::TFWorldTransformComponent>(current_entity);

    // Populate Debug State
    stats.depth = rel.depth;
    stats.version = world.version;
    stats.is_dirty = local.IsDirty();
    stats.has_dirty_children = rel.has_dirty_children;

    stats.local_position = local.GetPosition();
    stats.local_rotation = local.GetRotation();
    stats.local_scale    = local.GetScale();

    // Recursively build children by traversing the sibling chain
    entt::entity child = rel.first_child;
    while (child != entt::null) {
        stats.children.push_back(BuildNodeStatsRecursive(registry, child, visited, cyclical_nodes));
        
        // Advance to next sibling securely
        if (registry.all_of<rrl::tf::TFRelationshipComponent>(child)) {
            child = registry.get<rrl::tf::TFRelationshipComponent>(child).next_sibling;
        } else {
            break; // Child is missing its relationship component, break traversal for this branch
        }
    }

    return stats;
}


// --- API ---------------------------------------------------------
TFDebugReport GetTransformTreeDebugReport(entt::registry& registry) {
    TFDebugReport report;
    std::unordered_set<entt::entity> visited;

    auto view = registry.view<rrl::tf::TFRelationshipComponent>();

    for (auto entity : view) {
        // (A valid TF node must have all 3 components: Relation, World and Local)
        if (!registry.all_of<rrl::tf::TFLocalTransformComponent, rrl::tf::TFWorldTransformComponent>(entity)) {
            report.malformed_nodes.push_back( ToObjectID(entity) );
            continue;
        }

        report.total_valid_nodes++;

        // Locate all Root nodes to initiate tree building
        const auto& rel = view.get<rrl::tf::TFRelationshipComponent>(entity);
        if (rel.parent == entt::null) {
            report.root_nodes.push_back(BuildNodeStatsRecursive(registry, entity, visited, report.cyclical_nodes));
        }
    }

    // Catch detached cyclical graphs (Loops that have no root node at all)
    for (auto entity : view) {
        if (!visited.count(entity) && 
            std::find(report.malformed_nodes.begin(), report.malformed_nodes.end(), ToObjectID(entity)) == report.malformed_nodes.end()) {
            report.root_nodes.push_back(BuildNodeStatsRecursive(registry, entity, visited, report.cyclical_nodes));
        }
    }

    return report;
}


} // namespace rrl::debug::tf