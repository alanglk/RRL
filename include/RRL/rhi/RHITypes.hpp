// RRL/include/rhi/RHITypes.hpp
#pragma once

#include <RRL/rrl_export.h>
#include <cstdint>


namespace rrl::rhi {


/**
 * @brief Runtime flags for the RHI to toggle debug rendering features.
 */
enum class RHIDebugFlag : uint32_t {
    FLAG_NONE                   = 0,
    FLAG_DRAW_WIREFRAMES        = 1 << 0,   // Render meshes as wireframes (except point topology)
    FLAG_DISABLE_TEXTURES       = 1 << 1,   // Skip texture sampling (draw base colors only)
    FLAG_SHOW_UVS               = 1 << 2,   // Render UV coordinates as RGB colors
    FLAG_AFFINE_INTERPOLATION   = 1 << 3,   // Use Affine interpolation instead of baycentric interpolation (texture mapping)
};


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
enum class RHIRenderLayer : uint32_t {
    LAYER_NONE     = 0,
    LAYER_DEFAULT  = 1 << 0,  // Standard world objects (1)
    LAYER_UI       = 1 << 1,  // 2D UI elements (2)
    LAYER_DEBUG    = 1 << 2,  // Debug frustums, wireframes (4)
    LAYER_ALL      = 0xFFFFFFFF
};


/**
 * @brief Defines the rendering mode of the RHI
 */
enum class RHIWindowType : uint8_t {
    HEADLESS = 0,   // No window, renders to memmory
    OPENCV = 1,     // cv::imshow window
    GLFW = 2        // Hardware accelerated window
};


/**
 * @brief Types of rendering backends.
 */
enum class RHIBackendType : uint8_t {
    NONE = 0,
    SOFTWARE = 1,   // CPU SIMD Rasterizer (defaults to scalar if SIMD is not supported)
    OPENGL = 2      // GPU OpenGL FBOs
};


/**
 * @brief Generic OS Window Interface
 */
struct RRL_API RHIWindow {
    RHIWindowType type { RHIWindowType::HEADLESS };
    uint32_t width { 800 };
    uint32_t height { 600 };
    void* native_handle { nullptr }; // GLFWwindow* or const char* for OpenCV window name
};


// RHI rendering target ID (enable off-screen rendering support)
using RenderTargetHandle = uint32_t;
constexpr RenderTargetHandle TARGET_MAIN = 0;           // Handle 0 is reserved for the Screen / Main Target
constexpr RenderTargetHandle TARGET_NULL = 0xFFFFFFFF;  // Null handle


// RHIDebugFlag bitwise operations
inline constexpr RHIDebugFlag operator|(RHIDebugFlag a, RHIDebugFlag b) { 
    return static_cast<RHIDebugFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); 
}
inline constexpr RHIDebugFlag operator&(RHIDebugFlag a, RHIDebugFlag b) { 
    return static_cast<RHIDebugFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); 
}
inline constexpr RHIDebugFlag operator~(RHIDebugFlag a) { 
    return static_cast<RHIDebugFlag>(~static_cast<uint32_t>(a)); 
}
inline RHIDebugFlag& operator|=(RHIDebugFlag& a, RHIDebugFlag b) { 
    return a = a | b; 
}
inline RHIDebugFlag& operator&=(RHIDebugFlag& a, RHIDebugFlag b) { 
    return a = a & b; 
}


// RHIRenderLayer bitwise operations
inline constexpr RHIRenderLayer operator|(RHIRenderLayer a, RHIRenderLayer b) { 
    return static_cast<RHIRenderLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); 
}
inline constexpr RHIRenderLayer operator&(RHIRenderLayer a, RHIRenderLayer b) { 
    return static_cast<RHIRenderLayer>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); 
}
inline constexpr RHIRenderLayer operator~(RHIRenderLayer a) { 
    return static_cast<RHIRenderLayer>(~static_cast<uint32_t>(a)); 
}
inline RHIRenderLayer& operator|=(RHIRenderLayer& a, RHIRenderLayer b) { 
    return a = a | b; 
}
inline RHIRenderLayer& operator&=(RHIRenderLayer& a, RHIRenderLayer b) { 
    return a = a & b; 
}



} // namespace rrl::rhi