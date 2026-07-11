// RRL/src/include/RRL/data/MeshComponents.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace rrl::data {


/**
 * @brief CPU-side geometry data source.
 * Modify the mesh data here, then increment the version to trigger a RHI upload.
 */
struct MeshSourceComponent {
    std::shared_ptr<MeshData> mesh;
    std::atomic<uint32_t> version { 0 };
};

/**
 * @brief The RHI-side runtime cache. 
 * Managed entirely by the Render System. Do not touch from any other thread!
 */
struct MeshRuntimeComponent {
    rhi::MeshHandle handle { rhi::MESH_NULL };
    uint32_t cached_mesh_version { 0xFFFFFFFF };
};

/**
 * @brief Attached to physical world entities to link them to their 3D geometry.
 */
struct MeshLinkage {
    entt::entity mesh_asset { entt::null };
    
    // Maps 1:1 with MeshData::submeshes (it specifies the material of each submesh)
    std::vector<entt::entity> materials;

    // What layer does this physical object belong to?
    rhi::RHIRenderLayer layer_mask { rhi::RHIRenderLayer::LAYER_DEFAULT };
};

} // namespace rrl::data
