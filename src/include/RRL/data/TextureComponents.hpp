// RRL/src/include/RRL/data/TextureComponents.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace rrl::data {


/**
 * @brief CPU-side texture data source.
 * Modify the texture data here, then increment the version to trigger a RHI upload.
 */
struct TextureSourceComponent {
    std::shared_ptr<ImageData> image { nullptr };
    std::atomic<uint32_t> version { 0 };
};

/**
 * @brief The RHI-side runtime cache. 
 * Managed entirely by the Render System. Do not touch from any other thread!
 */
struct TextureRuntimeComponent {
    rhi::TextureHandle handle { rhi::TEXTURE_NULL };
    uint32_t cached_tex_version { 0xFFFFFFFF };
};

/**
 * @brief Attached to 2D UI/Screen entities to link them directly to a texture asset.
 */
struct TextureLinkage {
    entt::entity texture_asset { entt::null };
    
    // Normalized screen coordinates (0.0 to 1.0)
    // (0,0) is top-left, (1,1) is bottom-right.
    float screen_x { 0.0f };
    float screen_y { 0.0f };
    float screen_w { 1.0f }; // Full width default
    float screen_h { 1.0f }; // Full height default
    
    // What layer does this UI element belong to?
    rhi::RHIRenderLayer layer_mask { rhi::RHIRenderLayer::LAYER_UI };
};


} // namespace rrl::data
