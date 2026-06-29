#include <iostream>
#include <cstddef>
#include <new>

#include <glm/glm.hpp>
#include <hwy/highway.h>

constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

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

    // Links to an entity in the registry that holds a `MaterialData` component.
    // If entt::null, the RHI will use a default fallback material.
    uint32_t material_entity { 0xFFFFFFFF };
};

}



// SSE | AVX    -> 128-bit SIMD (16 bytes)
// AVX2         -> 256-bit SIMD (32 bytes)
#define RRL_SWR_SIMD_BIT_SIZE 128 
// #define SIMD_ALIGNMENT ( SIMD_BIT_SIZE / 8 ) // Minimum required alignment
#define RRL_SWR_SIMD_ALIGNMENT CACHE_LINE_SIZE // Cache aligment to avoid false cache sharing and ensuring one cache line fetch
#define RRL_SWR_SIMD_NUM_FLOATS ( RRL_SWR_SIMD_BIT_SIZE / ( 8 * sizeof(float) ) )

// #define RRL_SWR_FIXED_POINT


namespace rrl::rhi::software {


struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x2 {
    // Size (128-bit): 2 * 4 * 4 = 32 bytes
    float x[RRL_SWR_SIMD_NUM_FLOATS];
    float y[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x3 {
    // Size (128-bit): 3 * 4 * 4 + 16 (padding) = 64 bytes
    float x[RRL_SWR_SIMD_NUM_FLOATS];
    float y[RRL_SWR_SIMD_NUM_FLOATS];
    float z[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) f32x4 {
    // Size (128-bit): 4 * 4 * 4 = 64 bytes
    float x[RRL_SWR_SIMD_NUM_FLOATS];
    float y[RRL_SWR_SIMD_NUM_FLOATS];
    float z[RRL_SWR_SIMD_NUM_FLOATS];
    float w[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x2 {
    // Size (128-bit): 2 * 4 * 4 = 32 bytes
    int32_t x[RRL_SWR_SIMD_NUM_FLOATS];
    int32_t y[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x3 {
    // Size (128-bit): 3 * 4 * 4 + 16 (padding) = 64 bytes
    int32_t x[RRL_SWR_SIMD_NUM_FLOATS];
    int32_t y[RRL_SWR_SIMD_NUM_FLOATS];
    int32_t z[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) i32x4 {
    // Size (128-bit): 4 * 4 * 4 = 64 bytes
    int32_t x[RRL_SWR_SIMD_NUM_FLOATS];
    int32_t y[RRL_SWR_SIMD_NUM_FLOATS];
    int32_t z[RRL_SWR_SIMD_NUM_FLOATS];
    int32_t w[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x2 {
    // Size (128-bit): 2 * 4 * 4 = 32 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_FLOATS];
    uint32_t y[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x3 {
    // Size (128-bit): 3 * 4 * 4 + 16 (padding) = 64 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_FLOATS];
    uint32_t y[RRL_SWR_SIMD_NUM_FLOATS];
    uint32_t z[RRL_SWR_SIMD_NUM_FLOATS];
};
struct alignas(RRL_SWR_SIMD_ALIGNMENT) u32x4 {
    // Size (128-bit): 4 * 4 * 4 = 64 bytes
    uint32_t x[RRL_SWR_SIMD_NUM_FLOATS];
    uint32_t y[RRL_SWR_SIMD_NUM_FLOATS];
    uint32_t z[RRL_SWR_SIMD_NUM_FLOATS];
    uint32_t w[RRL_SWR_SIMD_NUM_FLOATS];
};



/*
Array of Structures of Arrays (AoSoA)
Structure of Arrays of Structures -> Tile rendering
We will group data as tiles: blocks of SIMD_BIT_SIZE
*/
struct SoftwareMesh {
    rrl::data::MeshTopology topology;
    size_t active_vertex_count { 0 };

    std::vector<uint32_t> indices;

    #ifndef RRL_SWR_FIXED_POINT
    std::vector<f32x3> positions;
    std::vector<f32x3> normals;     // Normals
    std::vector<f32x2> uvs;         // Texture mapping
    std::vector<f32x4> colors;      // Vertex colors
    #else
    std::vector<i32x3> positions;
    std::vector<i32x3> normals;     // Normals
    std::vector<i32x2> uvs;         // Texture mapping
    std::vector<i32x4> colors;      // Vertex colors
    #endif

    // Defines material groups. 
    std::vector<rrl::data::MeshMaterial> materials;
};



}












namespace simd_test {
namespace HWY_NAMESPACE {
    
namespace hn = hwy::HWY_NAMESPACE;

struct alignas(RRL_SWR_SIMD_ALIGNMENT) VertexBlock {
    // Positions
    float x[ RRL_SWR_SIMD_NUM_FLOATS ];     // 4 bytes * NUM_OF_SIMD_VEC_FLOATS
    float y[ RRL_SWR_SIMD_NUM_FLOATS ];     // 4 bytes * NUM_OF_SIMD_VEC_FLOATS
    float z[ RRL_SWR_SIMD_NUM_FLOATS ];     // 4 bytes * NUM_OF_SIMD_VEC_FLOATS

    // Normals
    float nx[ RRL_SWR_SIMD_NUM_FLOATS ];     // 4 bytes * NUM_OF_SIMD_VEC_FLOATS
    float ny[ RRL_SWR_SIMD_NUM_FLOATS ];     // 4 bytes * NUM_OF_SIMD_VEC_FLOATS
    float nz[ RRL_SWR_SIMD_NUM_FLOATS ];     // 4 bytes * NUM_OF_SIMD_VEC_FLOATS

    // UVs
    float u[ RRL_SWR_SIMD_NUM_FLOATS ];
    float v[ RRL_SWR_SIMD_NUM_FLOATS ];
};

void VertexProjection(VertexBlock& block, const glm::mat4& mvp_mat) {
    /*
        GLM Memory Layout: matrix[column][row]
        [0][0] [1][0] [2][0] [3][0]     X       ([0][0]*X) + ([1][0]*Y) + ([2][0]*Z) + ([3][0]*1.0)
        [0][1] [1][1] [2][1] [3][1]  *  Y   =   ([0][1]*X) + ([1][1]*Y) + ([2][1]*Z) + ([3][1]*1.0)
        [0][2] [1][2] [2][2] [3][2]     Z       ([0][2]*X) + ([1][2]*Y) + ([2][2]*Z) + ([3][2]*1.0)
        [0][3] [1][3] [2][3] [3][3]     1.0     ([0][3]*X) + ([1][3]*Y) + ([2][3]*Z) + ([3][3]*1.0)
    */
    
    hn::FixedTag<float, RRL_SWR_SIMD_NUM_FLOATS> d;
    
    auto v_x = hn::Load(d, block.x);
    auto v_y = hn::Load(d, block.x);
    auto v_z = hn::Load(d, block.x);
    
    // Clip Space X -> px = ([0][0]*X) + ([1][0]*Y) + ([2][0]*Z) + ([3][0]*1.0)
    auto clip_x = hn::Set(d, mvp_mat[3][0]);
    clip_x = hn::MulAdd(hn::Set(d, mvp_mat[0][0]), v_x, clip_x);
    clip_x = hn::MulAdd(hn::Set(d, mvp_mat[1][0]), v_y, clip_x);
    clip_x = hn::MulAdd(hn::Set(d, mvp_mat[2][0]), v_z, clip_x);
    
    // Clip Space Y -> py =([0][1]*X) + ([1][1]*Y) + ([2][1]*Z) + ([3][1]*1.0) 
    auto clip_y = hn::Set(d, mvp_mat[3][1]);
    clip_y = hn::MulAdd(hn::Set(d, mvp_mat[0][1]), v_x, clip_y);
    clip_y = hn::MulAdd(hn::Set(d, mvp_mat[1][1]), v_y, clip_y);
    clip_y = hn::MulAdd(hn::Set(d, mvp_mat[2][1]), v_z, clip_y);

    // Clip Space Z -> pz = ([0][2]*X) + ([1][2]*Y) + ([2][2]*Z) + ([3][2]*1.0)
    auto clip_z = hn::Set(d, mvp_mat[3][2]);
    clip_z = hn::MulAdd(hn::Set(d, mvp_mat[0][2]), v_x, clip_z);
    clip_z = hn::MulAdd(hn::Set(d, mvp_mat[1][2]), v_y, clip_z);
    clip_z = hn::MulAdd(hn::Set(d, mvp_mat[2][2]), v_z, clip_z);

    // Clip Space W -> pz = ([0][2]*X) + ([1][2]*Y) + ([2][2]*Z) + ([3][2]*1.0)
    auto clip_w = hn::Set(d, mvp_mat[3][3]);
    clip_w = hn::MulAdd(hn::Set(d, mvp_mat[0][3]), v_x, clip_w);
    clip_w = hn::MulAdd(hn::Set(d, mvp_mat[1][3]), v_y, clip_w);
    clip_w = hn::MulAdd(hn::Set(d, mvp_mat[2][3]), v_z, clip_w);
    
    // NDC -> Perspective division
    auto inv_w = hn::Div(hn::Set(d, 1.0f), clip_w);
    auto ndc_x = hn::Mul(clip_x, inv_w);
    auto ndc_y = hn::Mul(clip_y, inv_w);
    auto ndc_z = hn::Mul(clip_z, inv_w);


    // Save projection results
    hn::Store(ndc_x, d, block.x);
    hn::Store(ndc_y, d, block.y);
    hn::Store(ndc_z, d, block.z);

    // Store inv_w for baycentric interpolation?
}



}
}



int main() {
    
    int targets = hwy::SupportedTargets();
    std::cout << "Google Highway compiled successfully!\n";
    std::cout << "Supported CPU Targets Bitmask: " << targets << "\n";
    
    
    glm::mat4 mvp { 1.0f };
    simd_test::HWY_NAMESPACE::VertexBlock block = {};
    simd_test::HWY_NAMESPACE::VertexProjection(block, mvp);
    std::cout << "Result X0: " << block.x[0] << "\n";
    return 0;
}
