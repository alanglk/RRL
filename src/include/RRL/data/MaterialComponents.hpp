// RRL/src/include/RRL/data/MaterialComponents.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"


namespace rrl::data {

/**
 * @brief RHI runtime resolved links to material textures and RHI material handle
 */
struct MaterialRuntimeComponent {
    rhi::MaterialHandle handle { rhi::MATERIAL_NULL };

    rhi::TextureHandle albedo_handle { rhi::TEXTURE_NULL };
    rhi::TextureHandle normal_handle { rhi::TEXTURE_NULL };
    rhi::TextureHandle metallic_roughness_handle { rhi::TEXTURE_NULL };
    
};


} // namespace rrl::data
