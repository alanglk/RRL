// RRL/include/rhi/RHITypes.hpp
#pragma once

#include <RRL/rrl_export.h>
#include <cstdint>
#include <vector>
#include <functional> // For std::hash


// --- ResourceID --------------------------------------------------
namespace rrl::rhi {

// Compile-time FNV-1a 32-bit hash algorithm
namespace detail {
    constexpr uint32_t fnv1a_32(const char* s, std::size_t count) {
        uint32_t hash = 2166136261u;
        for (std::size_t i = 0; i < count; ++i) {
            hash ^= static_cast<uint8_t>(s[i]);
            hash *= 16777619u;
        }
        return hash;
    }
} // namespace detail


/**
 * @brief Identifier for all RHI resources (FBOs, Textures, Meshes, etc.)
 * Can be auto-generated or instantiated at compile-time via string literal.
 */
struct ResourceID {
    uint32_t id = 0;
    
    constexpr ResourceID() = default;
    constexpr explicit ResourceID(uint32_t val) : id(val) {}
    
    // Auto-hash string literals at compile time (e.g., ResourceID("SceneColor"))
    template <std::size_t N>
    constexpr ResourceID(const char (&str)[N]) : id(detail::fnv1a_32(str, N - 1)) {}
    
    constexpr bool operator==(const ResourceID& other) const { return id == other.id; }
    constexpr bool operator!=(const ResourceID& other) const { return id != other.id; }
};

constexpr ResourceID RESOURCE_NULL = ResourceID{0xFFFFFFFF};
constexpr ResourceID TARGET_MAIN   = ResourceID{0}; // The Screen Swapchain

} // namespace rrl::rhi



// Hash support for ResourceID to be used in std::unordered_map
template <>
struct std::hash<rrl::rhi::ResourceID> {
    std::size_t operator()(const rrl::rhi::ResourceID& k) const {
        return k.id;
    }
};



// --- RHI Types ---------------------------------------------------
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
enum class RHIRenderLayerMask : uint32_t {
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


/**
 * @brief Describes the attachments for a Render Target (FBO).
 * Textures must be allocated via the RHIBackend before creating the FBO.
 */
struct RenderTargetDescriptor {
    uint32_t width = 0;
    uint32_t height = 0;
    
    // Multiple Render Targets (MRT). OpenGL supports up to 8.
    std::vector<rrl::rhi::ResourceID> color_attachments; // We need another ID here that should be resolved on the rrl::rhi::CreateRenderTarget call to the actual rrl::rhi::TextureHandle
    
    // Optional: depth or depth-stencil attachment
    rrl::rhi::ResourceID depth_stencil_attachment = rrl::rhi::RESOURCE_NULL;
    
    // Optional: If true, this FBO targets a specific texture of a Texture Array
    bool is_texture_array = false;
    uint32_t array_idx = 0;  // The current rendering texture target on the color_attachments
};



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
inline constexpr RHIRenderLayerMask operator|(RHIRenderLayerMask a, RHIRenderLayerMask b) { 
    return static_cast<RHIRenderLayerMask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); 
}
inline constexpr RHIRenderLayerMask operator&(RHIRenderLayerMask a, RHIRenderLayerMask b) { 
    return static_cast<RHIRenderLayerMask>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); 
}
inline constexpr RHIRenderLayerMask operator~(RHIRenderLayerMask a) { 
    return static_cast<RHIRenderLayerMask>(~static_cast<uint32_t>(a)); 
}
inline RHIRenderLayerMask& operator|=(RHIRenderLayerMask& a, RHIRenderLayerMask b) { 
    return a = a | b; 
}
inline RHIRenderLayerMask& operator&=(RHIRenderLayerMask& a, RHIRenderLayerMask b) { 
    return a = a & b; 
}



} // namespace rrl::rhi