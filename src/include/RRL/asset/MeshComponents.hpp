// RRL/src/include/RRL/asset/MeshComponents.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace rrl::asset {


/**
 * @brief CPU-side geometry data source.
 * Modify the mesh data here, then increment the version to trigger a RHI upload.
 */
struct MeshSourceComponent {
    std::shared_ptr<MeshAsset> mesh;
    std::atomic<uint32_t> version { 0 };
};

/**
 * @brief The RHI-side runtime cache. 
 * Managed entirely by the Render System. Do not touch from any other thread!
 */
struct MeshRuntimeComponent {
    rhi::MeshHandle handle { rhi::BACKEND_MESH_NULL };
    uint32_t cached_mesh_version { 0xFFFFFFFF };
};

/**
 * @brief Attached to physical world entities to link them to their 3D geometry.
 */
struct MeshLinkage {
    entt::entity mesh_asset { entt::null };
    
    // Maps 1:1 with MeshAsset::submeshes (it specifies the material of each submesh)
    std::vector<entt::entity> materials;

    // What layer does this physical object belong to?
    rhi::RHIRenderLayerMask layer_mask { rhi::RHIRenderLayerMask::LAYER_DEFAULT };
};

} // namespace rrl::asset
