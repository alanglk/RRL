// RRL/include/rhi/RHIBackend.hpp
#pragma once

#include <entt/entt.hpp>

#include "RRL/data/ImageData.hpp"
#include "RRL/data/MeshData.hpp"
#include "RRL/data/MaterialData.hpp"

#include <cstdint>
#include <string>


namespace rrl::rhi {

// RHI rendering target ID (enable off-screen rendering support)
using RenderTargetHandle = uint32_t;
constexpr RenderTargetHandle TARGET_MAIN = 0;           // Handle 0 is reserved for the Screen / Main Target
constexpr RenderTargetHandle TARGET_NULL = 0xFFFFFFFF;  // Null handle

// RHI texture handling (loaded/allocated textures)
using TextureHandle = uint32_t;
constexpr TextureHandle TEXTURE_NULL = 0xFFFFFFFF;  // Null handle

// RHI mesh handling (loaded/allocated meshes)
using MeshHandle = uint32_t;
constexpr MeshHandle MESH_NULL = 0xFFFFFFFF;  // Null handle

// RHI material handling (UBO tracking)
using MaterialHandle = uint32_t;
constexpr MaterialHandle MATERIAL_NULL = 0xFFFFFFFF;  // Null handle



/**
 * @brief Types of rendering backends.
 */
enum class RHIBackendType : uint8_t {
    NONE = 0,
    OPENCV = 1      // Use OpenCV for rendering
};

/**
 * @brief Defines the rendering mode of the RHI
 */
enum class RHIRenderingMode : uint8_t {
    WINDOW = 0,     // Opens an OS window (cv::imshow, GLFW)
    HEADLESS = 1    // Pure background rendering 
};

/**
 * @brief Runtime flags for the RHI to toggle debug rendering features.
 */
enum class RHIDebugFlag : uint32_t {
    FLAG_NONE              = 0,
    FLAG_DRAW_WIREFRAMES   = 1 << 0,  // Render meshes as wireframes (except point topology)
    FLAG_DISABLE_TEXTURES  = 1 << 1,  // Skip texture sampling (draw base colors only)
    FLAG_SHOW_UVS          = 1 << 2   // Render UV coordinates as RGB colors
};

/**
 * @brief Defines how the RHI context is created.
 */
struct RHIConfig {
    uint32_t width { 800 };
    uint32_t height { 600 };
    std::string title { "RRL Context" };
    RHIRenderingMode mode { RHIRenderingMode::WINDOW };
};

/**
 * @brief Dispatch table for rendering backends.
 */
struct RHIBackend {
    RHIBackendType type {RHIBackendType::NONE};
    
    // Function pointers (defined for each backend type)
    
    // Lifecycle
    bool (*Initialize)(entt::registry& registry, const RHIConfig& config) { nullptr };
    void (*Shutdown)(entt::registry& registry) { nullptr };
    void (*RenderFrame)(entt::registry& registry) { nullptr };
    
    // Target FBOs
    RenderTargetHandle (*CreateRenderTarget)(entt::registry& registry, uint32_t width, uint32_t height) { nullptr };
    void (*DestroyRenderTarget)(entt::registry& registry, RenderTargetHandle handle) { nullptr };

    // Textures
    TextureHandle (*CreateTexture)(entt::registry& registry, const data::ImageData& image_data) { nullptr };
    void (*UpdateTexture)(entt::registry& registry, TextureHandle handle, const data::ImageData& image_data) { nullptr };
    void (*DestroyTexture)(entt::registry& registry, TextureHandle handle) { nullptr };
    
    // Meshes
    MeshHandle (*CreateMesh)(entt::registry& registry, const data::MeshData& mesh_data) { nullptr };
    void (*UpdateMesh)(entt::registry& registry, MeshHandle handle, const data::MeshData& mesh_data) { nullptr };
    void (*DestroyMesh)(entt::registry& registry, MeshHandle handle) { nullptr };


    // --- Materials ---
    MaterialHandle (*CreateMaterial)(entt::registry& registry, const data::MaterialData& material_data) { nullptr };
    void (*UpdateMaterial)(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data) { nullptr };
    void (*DestroyMaterial)(entt::registry& registry, MaterialHandle handle) { nullptr };


    // Reads rendered data from the RHI back to CPU RAM
    data::ImageData (*GetTargetImage)(entt::registry& registry, RenderTargetHandle handle) { nullptr };


    // --- Debugging ---
    void (*SetDebugFlag)(entt::registry& registry, RHIDebugFlag flag, bool enable) { nullptr };
    RHIDebugFlag (*GetActiveDebugFlags)(entt::registry& registry) { nullptr };

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


} // namespace rrl::rhi 

