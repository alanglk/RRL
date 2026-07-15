// RRL/include/io/PrefabIO.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/asset/MeshAsset.hpp"
#include "RRL/asset/MaterialAsset.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>


namespace rrl::io {

/**
 * @brief File loaded material. This contains references for calling io::LoadImage().
 */
struct RRL_API IOMaterial {
    std::string name;       // Material name from the parsed file
    rrl::asset::MaterialAsset material_parameters; // only fills static values
    std::string albedo_path;
    std::string normal_path;
    std::string metallic_roughness_path;
    std::string emissive_path;
};

/**
 * @brief A file-loaded hierarchical node (can contain a mesh, a transform, and children).
 */
struct RRL_API IONode {
    std::string name;       // Mesh node name for instancing / identification
    rrl::asset::MeshAsset mesh;    // Node mesh (can be empty if it's just a transform group)
    
    // Maps 1:1 with mesh.submeshes. Stores the parsed string names of the materials.
    std::vector<std::string> submesh_material_names;
    
    // Node default relative transform to its parent
    glm::vec3 local_position {0.0f};
    glm::quat local_rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 local_scale {1.0f};
    
    // Nested children nodes
    std::vector<IONode> children;
};

/**
 * @brief File loaded complete scene (hierarchy and materials)
 */
struct RRL_API IOPrefab {
    std::string filepath; // for caching
    std::vector<IOMaterial> materials;
    std::vector<IONode> root_nodes; // hierarchy
};;

/**
 * @brief Loads prefabs data from a file.
 * Automatically detects the file format and uses the appropiate parser.
 * This does not load associated textures directly. Use io::LoadImage() for that.
 * Supported formats:
 *  - .obj
 */
rrl::io::IOPrefab RRL_API LoadPrefab(const std::string& filepath);




} // namespace rrl::io