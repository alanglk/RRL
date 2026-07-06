// RRL/src/include/RRL/data/MaterialComponents.hpp
#pragma once

#include "RRL/data/MaterialData.hpp"

#include "RRL/rhi/RHIBackend.hpp"
#include <atomic>


namespace rrl::data {

    
/**
 * @brief CPU-side material data source.
 * Modify the material data here, then increment the version to trigger a RHI sync.
 */
struct MaterialSourceComponent {
    MaterialData data; // inline storage as it is lightweight
    std::atomic<uint32_t> version { 0 };
};

/**
 * @brief RHI runtime resolved links to material textures and RHI material handle
 */
struct MaterialRuntimeComponent {
    rhi::MaterialHandle handle { rhi::MATERIAL_NULL };

    rhi::TextureHandle albedo_handle { rhi::TEXTURE_NULL };
    rhi::TextureHandle normal_handle { rhi::TEXTURE_NULL };
    rhi::TextureHandle metallic_roughness_handle { rhi::TEXTURE_NULL };
    rhi::TextureHandle emissive_hangle { rhi::TEXTURE_NULL };
    
    uint32_t cached_mat_version { 0xFFFFFFFF };
};


} // namespace rrl::data
