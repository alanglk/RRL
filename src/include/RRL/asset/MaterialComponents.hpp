// RRL/src/include/RRL/asset/MaterialComponents.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"
#include <atomic>


namespace rrl::asset {

    
/**
 * @brief CPU-side material data source.
 * Modify the material data here, then increment the version to trigger a RHI sync.
 */
struct MaterialSourceComponent {
    MaterialAsset data; // inline storage as it is lightweight
    std::atomic<uint32_t> version { 0 };
};

/**
 * @brief RHI runtime resolved links to material textures and RHI material handle
 */
struct MaterialRuntimeComponent {
    rhi::MaterialHandle handle { rhi::BACKEND_MATERIAL_NULL };

    rhi::TextureHandle albedo_handle { rhi::BACKEND_MATERIAL_NULL };
    rhi::TextureHandle normal_handle { rhi::BACKEND_MATERIAL_NULL };
    rhi::TextureHandle metallic_roughness_handle { rhi::BACKEND_MATERIAL_NULL };
    rhi::TextureHandle emissive_hangle { rhi::BACKEND_MATERIAL_NULL };
    
    uint32_t cached_mat_version { 0xFFFFFFFF };
};


} // namespace rrl::asset
