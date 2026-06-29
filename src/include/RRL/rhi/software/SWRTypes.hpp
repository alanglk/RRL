// RRL/include/rhi/software/SWRTypes.hpp
#pragma once

#include "RRL/rhi/software/SIMDTypes.hpp"
#include "RRL/data/MeshData.hpp"

namespace rrl::rhi::software {

/*
Array of Structures of Arrays (AoSoA)
Structure of Arrays of Structures -> Tile rendering
We will group data as tiles: blocks of SIMD_BIT_SIZE
*/
struct SWRMesh {
    static constexpr size_t BlockSize = RRL_SWR_SIMD_NUM_32BIT_ELMS;

    rrl::data::MeshTopology topology;
    size_t active_vertex_count { 0 };

    std::vector<uint32_t> indices;

    std::vector<swr_vec3> positions;
    std::vector<swr_vec3> normals;     // Normals
    std::vector<swr_vec2> uvs;         // Texture mapping
    std::vector<swr_vec4> colors;      // Vertex colors

    #ifndef RRL_SWR_AFFINE_INTERPOLATION
    std::vector<swr_vec1> inv_w;    // If using perspective-correct interpolation | else default to affine interpolation
    #endif

    // Defines material groups. 
    std::vector<rrl::data::MeshMaterial> materials;
};


struct SWRVertexBuffer {
    // x, y, z = Normalized Device Coordinates (NDC)
    // w = original clip space W (frustum culling)
    std::vector<glm::vec4> ndc_positions; 

    // projection computed 'inv depth'
    #ifndef RRL_SWR_AFFINE_INTERPOLATION
    std::vector<float> inv_w;
    #endif

    void Resize(size_t vertex_count) {
        if (ndc_positions.size() < vertex_count) {
            ndc_positions.resize(vertex_count);
            #ifndef RRL_SWR_AFFINE_INTERPOLATION
            inv_w.resize(vertex_count);
            #endif
        }
    }

};


} // namespace rrl::rhi::software