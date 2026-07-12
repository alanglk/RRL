// RRL/include/debug/TFDebugTypes.hpp
#pragma once

#include <RRL/rrl_export.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "RRL/RRLTypes.hpp"

namespace rrl::debug {


// --- Debug Structs -----------------------------------------------
struct RRL_API TFNodeDebugStats {
    ObjectID object_id;
    
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

struct RRL_API TFDebugReport {
    // Valid trees root nodes
    std::vector<TFNodeDebugStats> root_nodes;
    
    // Entities that have a TFRelationshipComponent, but are missing the actual TF components.
    std::vector<ObjectID> malformed_nodes; 
    
    // ERROR: Entities caught in a parent/child infinite loop
    std::vector<ObjectID> cyclical_nodes;  
    
    uint32_t total_valid_nodes {0};
};


} // namespace rrl::debug

