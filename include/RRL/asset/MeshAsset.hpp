// RRL/include/data/MeshAsset.hpp
#pragma once

#include <RRL/rrl_export.h>

#include <glm/fwd.hpp>

#include <cstdint>
#include <vector>

namespace rrl::asset {
    

/**
 * @brief Defines the raw memory layout (tells the RHI how to interpret the data).
 */
enum class MeshTopology : uint8_t {
    POINTS      = 0,    // Point clouds
    LINES       = 1,    // Wireframes
    TRIANGLES   = 2     // Solid meshes
};


/**
 * @brief Defines a sub-section of the mesh geometry.
 *  These sections can be mapped to different materials. See 
 *      rrl::asset::BindMesh()
 */
struct MeshSubmesh {
    uint32_t index_offset { 0 };
    uint32_t index_count { 0 };
};


/**
 * @brief Geometry data container. SoA layout where 
 * non provided attributes remain as empty vectors.
 */
struct RRL_API MeshAsset {
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

    
    // Defines submesh groups. 
    std::vector<MeshSubmesh> submeshes;
};


} // namespace rrl::asset