// RRL/include/rhi/RHILayers.hpp
#pragma once

#include <cstdint>


namespace rrl::rhi {


/**
 * @brief Culling mask system. 
 * This tags physical objects to tell cameras how to render them.
 * * @example
 *  - 1. Assign an object to multiple layers (e.g., UI and Debug)
 * mesh.layer_mask = rhi::LAYER_UI | rhi::LAYER_DEBUG;
 *  - 2. Setup a camera to see the Default world AND Debug lines
 * camera.culling_mask = rhi::LAYER_DEFAULT | rhi::LAYER_DEBUG;
 *  - 3. The RHI evaluates visibility using a bitwise AND
 */
enum class RenderLayer : uint32_t {
    LAYER_NONE     = 0,
    LAYER_DEFAULT  = 1 << 0,  // Standard world objects (1)
    LAYER_UI       = 1 << 1,  // 2D UI elements (2)
    LAYER_DEBUG    = 1 << 2,  // Debug frustums, wireframes (4)
    LAYER_ALL      = 0xFFFFFFFF
};


// RenderLayer bitwise operations
inline constexpr RenderLayer operator|(RenderLayer a, RenderLayer b) { 
    return static_cast<RenderLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); 
}
inline constexpr RenderLayer operator&(RenderLayer a, RenderLayer b) { 
    return static_cast<RenderLayer>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); 
}
inline constexpr RenderLayer operator~(RenderLayer a) { 
    return static_cast<RenderLayer>(~static_cast<uint32_t>(a)); 
}
inline RenderLayer& operator|=(RenderLayer& a, RenderLayer b) { 
    return a = a | b; 
}
inline RenderLayer& operator&=(RenderLayer& a, RenderLayer b) { 
    return a = a & b; 
}


}