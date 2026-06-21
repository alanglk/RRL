#pragma once

#include "glm/fwd.hpp"
#include <cstdint>
#include <vector>

namespace rrl::data {
    

/**
 * @brief Defines the raw memory layout (tells the RHI how to interpret the data).
 */
enum class MeshTopology : uint8_t {
    POINTS      = 0,    // Point clouds
    LINES       = 1,    // Wireframes
    TRIANGLES   = 2     // Solid meshes
};


/**
 * @brief Defines a sub-section of the mesh that uses a specific material.
 */
struct MeshMaterial {
    uint32_t index_offset { 0 };
    uint32_t index_count { 0 };
    // entt::hashed_string material_id; 
};


/**
 * @brief Geometry data container. SoA layout where 
 * non provided attributes remain as empty vectors.
 */
struct MeshData {
    MeshTopology topology { MeshTopology::TRIANGLES };

    // Geometry 
    std::vector<glm::vec3> positions;

    // Indexing
    std::vector<uint32_t> indices;

    
    // Shading Attributes 
    std::vector<glm::vec3> normals;     // Normals
    std::vector<glm::vec2> uvs;         // Texture mapping
    std::vector<glm::vec4> tangents;    // Normal mapping
    std::vector<glm::vec4> colors;      // Vertex colors


    // Skeletal Animation Attributes 
    std::vector<glm::ivec4> bone_ids;
    std::vector<glm::vec4> bone_weights;

    
    // Defines material groups. 
    std::vector<MeshMaterial> materials;
};


} // namespace rrl::data