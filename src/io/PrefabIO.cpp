// RRL/src/io/PrefabIO.cpp
#include "RRL/io/PrefabIO.hpp"

#include <FLogging/FLogging.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>

// The default obj loader max size is 256Mb. If you need to increase the max loading size use this:
#ifndef TINYOBJLOADER_STREAM_READER_MAX_BYTES
// #define TINYOBJLOADER_STREAM_READER_MAX_BYTES (size_t(256) * size_t(1024) * size_t(1024))
#define TINYOBJLOADER_STREAM_READER_MAX_BYTES (size_t(265) * size_t(1024) * size_t(1024))
#endif
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
        IOMaterial parsed_mat {.name = tiny_mat.name };
        
        // PBR albedo = Phong diffuse
        parsed_mat.material_parameters.base_color = glm::vec4(
            tiny_mat.diffuse[0], tiny_mat.diffuse[1], tiny_mat.diffuse[2], 1.0f
        );

        // Emission
        parsed_mat.material_parameters.emmission = glm::vec3(
            tiny_mat.emission[0], tiny_mat.emission[1], tiny_mat.emission[2]
        );

        // PBR Attributes (tinyobjloader supports modern PBR extensions)
        parsed_mat.material_parameters.roughness    = tiny_mat.roughness; // Pr
        parsed_mat.material_parameters.metallic     = tiny_mat.metallic;   // Pm

        // Texture Mapping
        if (!tiny_mat.diffuse_texname.empty()) {
            parsed_mat.albedo_path = config.mtl_search_path + tiny_mat.diffuse_texname;
        }

        // Normal or bump textures for normal mapping
        if (!tiny_mat.normal_texname.empty()) {
            parsed_mat.normal_path = config.mtl_search_path + tiny_mat.normal_texname;
        }
        else if (!tiny_mat.bump_texname.empty()) {
            parsed_mat.normal_path = config.mtl_search_path + tiny_mat.bump_texname;
        } 

        // PBR metallic/roughness textures ( we may need to combine both )
        if (!tiny_mat.roughness_texname.empty()) {
            parsed_mat.metallic_roughness_path = config.mtl_search_path + tiny_mat.roughness_texname;
        } else if (!tiny_mat.metallic_texname.empty()) {
            parsed_mat.metallic_roughness_path = config.mtl_search_path + tiny_mat.metallic_texname;
        }
        
        // Emissive Map
        if (!tiny_mat.emissive_texname.empty()) {
            parsed_mat.emissive_path = config.mtl_search_path + tiny_mat.emissive_texname;
        }

        scene.materials.push_back(parsed_mat);
    }

    // Load Geometry
    for (const auto& tiny_shape : tiny_shapes) {
        
        // Changed IOShape to IONode
        IONode parsed_node; 
        parsed_node.name = tiny_shape.name;
        parsed_node.mesh.topology = data::MeshTopology::TRIANGLES;


        std::transform(parsed_node.name.begin(), parsed_node.name.end(), parsed_node.name.begin(), ::tolower);
        std::replace(parsed_node.name.begin(), parsed_node.name.end(), '.', '_');
        if (parsed_node.name.empty()) parsed_node.name = "unnamed_shape";

        std::unordered_map<VertexTuple, uint32_t> unique_vertices;
        int current_material_id = -1;
        uint32_t current_index_offset = 0;
        uint32_t current_index_count = 0;
        size_t index_offset = 0;
        
        // Lambda to pack a name into a temporary entity identifyer
        auto GetMaterialHashAsEntity = [&](int mat_id) -> entt::entity {
            if (mat_id >= 0 && mat_id < static_cast<int>(tiny_materials.size())) {
                entt::id_type hash = entt::hashed_string(tiny_materials[mat_id].name.c_str()).value();
                return static_cast<entt::entity>(hash);
            }
            return entt::null;
        };

        for (size_t f = 0; f < tiny_shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(tiny_shape.mesh.num_face_vertices[f]);
            int face_mat_id = tiny_shape.mesh.material_ids[f];

            // Sub-mesh boundary detection based on material ID changes
            if (face_mat_id != current_material_id) {
                if (current_index_count > 0) {
                    parsed_node.mesh.materials.push_back({
                        current_index_offset, 
                        current_index_count, 
                        GetMaterialHashAsEntity(current_material_id)
                    });
                }
                current_material_id = face_mat_id;
                current_index_offset = static_cast<uint32_t>(parsed_node.mesh.indices.size());
                current_index_count = 0;
            }

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = tiny_shape.mesh.indices[index_offset + v];
                VertexTuple tuple = {idx.vertex_index, idx.texcoord_index, idx.normal_index};

                // Deduplicate vertices using the hash map
                if (unique_vertices.count(tuple) == 0) {
                    unique_vertices[tuple] = static_cast<uint32_t>(parsed_node.mesh.positions.size());
                    
                    // Convert Y-Up (OBJ) to Z-Up (ISO 8855 Standard)
                    // X_out = X_in ; Y_out = -Z_in ; Z_out = Y_in
                    parsed_node.mesh.positions.push_back(glm::vec3(
                         tiny_attrib.vertices[3 * idx.vertex_index + 0], 
                        -tiny_attrib.vertices[3 * idx.vertex_index + 2], // Z becomes inverted Y
                         tiny_attrib.vertices[3 * idx.vertex_index + 1]  // Y becomes Z (Up)
                    ));
                    
                    if (idx.normal_index >= 0) {
                        parsed_node.mesh.normals.push_back(glm::vec3(
                             tiny_attrib.normals[3 * idx.normal_index + 0], 
                            -tiny_attrib.normals[3 * idx.normal_index + 2], 
                             tiny_attrib.normals[3 * idx.normal_index + 1]
                        ));
                    }
                    
                    /*
                    // Standard loading
                    parsed_node.mesh.positions.push_back(glm::vec3(
                        tiny_attrib.vertices[3 * idx.vertex_index + 0], 
                        tiny_attrib.vertices[3 * idx.vertex_index + 1], 
                        tiny_attrib.vertices[3 * idx.vertex_index + 2]
                    ));

                    if (idx.normal_index >= 0) {
                        parsed_node.mesh.normals.push_back(glm::vec3(
                            tiny_attrib.normals[3 * idx.normal_index + 0], 
                            tiny_attrib.normals[3 * idx.normal_index + 1], 
                            tiny_attrib.normals[3 * idx.normal_index + 2]
                        ));
                    }
                    */
                    
                    if (idx.texcoord_index >= 0) {
                        parsed_node.mesh.uvs.push_back(glm::vec2(
                            tiny_attrib.texcoords[2 * idx.texcoord_index + 0], 
                            1.0f - tiny_attrib.texcoords[2 * idx.texcoord_index + 1]
                        ));
                    }
                    
                    if (!tiny_attrib.colors.empty()) {
                        parsed_node.mesh.colors.push_back(glm::vec4(
                            tiny_attrib.colors[3 * idx.vertex_index + 0], 
                            tiny_attrib.colors[3 * idx.vertex_index + 1], 
                            tiny_attrib.colors[3 * idx.vertex_index + 2], 
                            1.0f
                        ));
                    }
                }
                
                parsed_node.mesh.indices.push_back(unique_vertices[tuple]);
                current_index_count++;
            }
            index_offset += fv;
        }

        // Push the final sub-mesh material group
        if (current_index_count > 0) {
            parsed_node.mesh.materials.push_back({
                current_index_offset, 
                current_index_count, 
                GetMaterialHashAsEntity(current_material_id)
            });
        }
        
        // Push the fully assembled node to the root
        scene.root_nodes.push_back(std::move(parsed_node));
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

