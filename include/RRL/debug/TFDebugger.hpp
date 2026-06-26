// RRL/include/debug/TFDebugger.cpp
#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace rrl::debug::tf {


// --- Debug Structs -----------------------------------------------
struct TFNodeDebugStats {
    entt::entity entity_id;
    
    // Hierarchy Metadata
    uint32_t depth;
    
    // Engine State Flags
    uint32_t version;
    bool is_dirty;
    bool has_dirty_children;

    // Transform Data
    glm::vec3 local_position;
    glm::quat local_rotation;
    glm::vec3 local_scale;

    // Hierarchy
    std::vector<TFNodeDebugStats> children;
};

struct TFDebugReport {
    // Valid trees root nodes
    std::vector<TFNodeDebugStats> root_nodes;
    
    // Entities that have a TFRelationshipComponent, but are missing the actual TF components.
    std::vector<entt::entity> malformed_nodes; 
    
    // ERROR: Entities caught in a parent/child infinite loop
    std::vector<entt::entity> cyclical_nodes;  
    
    uint32_t total_valid_nodes {0};
};


// --- API ---------------------------------------------------------
/**
 * @brief Parses the ECS registry to reconstruct the Transform Tree and evaluate its health.
 */
TFDebugReport GetTransformTreeDebugReport(entt::registry& registry);

} // namespace rrl::debug::tf
