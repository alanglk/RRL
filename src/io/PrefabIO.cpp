// RRL/src/io/PrefabIO.cpp
#include "RRL/io/PrefabIO.hpp"

#include <FLogging/FLogging.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif
#include <tiny_obj_loader.h>


using namespace rrl::data;
namespace fs = std::filesystem;


// Hash to combine v/vt/vn into a single index: 
//  .obj indices -> MeshData indices
struct VertexTuple {
    int v, vt, vn;
    bool operator==(const VertexTuple& other) const {
        return v == other.v && vt == other.vt && vn == other.vn;
    }
};
namespace std {
    template<> struct hash<VertexTuple> {
        size_t operator()(const VertexTuple& t) const {
            return ((hash<int>()(t.v) ^ (hash<int>()(t.vt) << 1)) >> 1) ^ (hash<int>()(t.vn) << 1);
        }
    };
}



namespace rrl::io {



// --- Obj Files ---------------------------------------------------
static IOPrefab LoadObjPrefab(const std::string& filepath) {
    IOPrefab scene {.filepath = filepath};
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    config.vertex_color = true;

    // Resolve base path for material and texture relative lookups
    std::filesystem::path obj_path(filepath);
    config.mtl_search_path = obj_path.parent_path().string() + "/"; 


    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filepath, config)) {
        if (!reader.Error().empty()) {
            LOG_ERROR("[MeshIO] TinyObjReader Error: {}", reader.Error());
        }
        return scene;
    }

    if (!reader.Warning().empty()) {
        LOG_WARN("[MeshIO] TinyObjReader Warning: {}", reader.Warning());
    }

    auto& tiny_attrib = reader.GetAttrib();
    auto& tiny_shapes = reader.GetShapes();
    auto& tiny_materials = reader.GetMaterials();

    // Load Materials
    for (const auto& tiny_mat : tiny_materials) {
        IOMaterial parsed_mat;
        
        // TODO: Add more tiny_mat material elements
        

        parsed_mat.material_parameters.base_color = glm::vec4(
            tiny_mat.diffuse[0], tiny_mat.diffuse[1], tiny_mat.diffuse[2], 1.0f
        );
        parsed_mat.material_parameters.roughness = tiny_mat.roughness;
        parsed_mat.material_parameters.metallic = tiny_mat.metallic;

        if (!tiny_mat.diffuse_texname.empty()) {
            parsed_mat.albedo_path = config.mtl_search_path + tiny_mat.diffuse_texname;
        }
        if (!tiny_mat.normal_texname.empty()) {
            parsed_mat.normal_path = config.mtl_search_path + tiny_mat.normal_texname;
        }

        scene.materials.push_back(parsed_mat);
    }

    // Load Geometry
    for (const auto& tiny_shape : tiny_shapes) {
        IOShape parsed_shape;
        parsed_shape.name = tiny_shape.name;
        parsed_shape.mesh.topology = data::MeshTopology::TRIANGLES;

        std::unordered_map<VertexTuple, uint32_t> unique_vertices;
        int current_material_id = -1;
        uint32_t current_index_offset = 0;
        uint32_t current_index_count = 0;
        size_t index_offset = 0;

        for (size_t f = 0; f < tiny_shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(tiny_shape.mesh.num_face_vertices[f]);
            int face_mat_id = tiny_shape.mesh.material_ids[f];

            // Sub-mesh boundary detection based on material ID changes
            if (face_mat_id != current_material_id) {
                if (current_index_count > 0) {
                    parsed_shape.mesh.materials.push_back({
                        current_index_offset, 
                        current_index_count, 
                        static_cast<entt::entity>(current_material_id) // Temporary index cast
                    });
                }
                current_material_id = face_mat_id;
                current_index_offset = static_cast<uint32_t>(parsed_shape.mesh.indices.size());
                current_index_count = 0;
            }

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = tiny_shape.mesh.indices[index_offset + v];
                VertexTuple tuple = {idx.vertex_index, idx.texcoord_index, idx.normal_index};

                // Deduplicate vertices using the hash map
                if (unique_vertices.count(tuple) == 0) {
                    unique_vertices[tuple] = static_cast<uint32_t>(parsed_shape.mesh.positions.size());
                    
                    // Positions
                    parsed_shape.mesh.positions.push_back(glm::vec3(
                        tiny_attrib.vertices[3 * idx.vertex_index + 0], 
                        tiny_attrib.vertices[3 * idx.vertex_index + 1], 
                        tiny_attrib.vertices[3 * idx.vertex_index + 2]
                    ));
                    
                    // Normals
                    if (idx.normal_index >= 0) {
                        parsed_shape.mesh.normals.push_back(glm::vec3(
                            tiny_attrib.normals[3 * idx.normal_index + 0], 
                            tiny_attrib.normals[3 * idx.normal_index + 1], 
                            tiny_attrib.normals[3 * idx.normal_index + 2]
                        ));
                    }
                    
                    // UVs (Inverted Y coordinate for standard texture mapping)
                    if (idx.texcoord_index >= 0) {
                        parsed_shape.mesh.uvs.push_back(glm::vec2(
                            tiny_attrib.texcoords[2 * idx.texcoord_index + 0], 
                            1.0f - tiny_attrib.texcoords[2 * idx.texcoord_index + 1]
                        ));
                    }
                    
                    // Colors
                    if (!tiny_attrib.colors.empty()) {
                        parsed_shape.mesh.colors.push_back(glm::vec4(
                            tiny_attrib.colors[3 * idx.vertex_index + 0], 
                            tiny_attrib.colors[3 * idx.vertex_index + 1], 
                            tiny_attrib.colors[3 * idx.vertex_index + 2], 
                            1.0f
                        ));
                    }
                }
                
                parsed_shape.mesh.indices.push_back(unique_vertices[tuple]);
                current_index_count++;
            }
            index_offset += fv;
        }

        // Push the final sub-mesh material group
        if (current_index_count > 0) {
            parsed_shape.mesh.materials.push_back({
                current_index_offset, 
                current_index_count, 
                static_cast<entt::entity>(current_material_id)
            });
        }
        
        scene.shapes.push_back(std::move(parsed_shape));
    }

    return scene;
}


// --- I/O ---------------------------------------------------------
IOPrefab LoadPrefab(const std::string& filepath) {

    // Check filepath
    if(!fs::exists(filepath)) {
        LOG_ERROR("LoadMesh failed. Mesh path '{}' does not exist.", filepath);
        return {};
    }

    // Extract extension and convert to lowercase
    fs::path path(filepath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Dynamic Factory Detection
    if (ext == ".obj") return LoadObjPrefab(filepath);
    // else if (ext == ".pcd") return LoadPcdFile(filepath, mesh_data);
    // else if (ext == ".gltf") return LoadGltfFile(filepath, mesh_data);
    
    
    LOG_ERROR("LoadMesh failed. Unsupported format or datatype mismatch.");
    return {};
}




} // namespace rrl::io







