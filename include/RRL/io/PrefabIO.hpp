// RRL/include/io/PrefabIO.hpp
#pragma once

#include "RRL/data/MeshData.hpp"
#include "RRL/data/MaterialData.hpp"
#include <string>
#include <vector>


namespace rrl::io {

/**
 * @brief File loaded material. This contains references for calling io::LoadImage().
 */
struct IOMaterial {
    data::MaterialData material_parameters; // only fills static values
    std::string albedo_path;
    std::string normal_path;
};

/**
 * @brief File loaded shape (mesh)
 */
struct IOShape {
    std::string name;       // Shape name. For instancing
    data::MeshData mesh;    // Shape mesh
};

/**
 * @brief File loaded complete scene (shapes and materials)
 */
struct IOPrefab {
    std::string filepath; // for caching
    std::vector<IOMaterial> materials;
    std::vector<IOShape> shapes;
};

/**
 * @brief Loads prefabs data from a file.
 * Automatically detects the file format and uses the appropiate parser.
 * This does not load associated textures directly. Use io::LoadImage() for that.
 * Supported formats:
 *  - .obj
 */
IOPrefab LoadPrefab(const std::string& filepath);




} // namespace rrl::io