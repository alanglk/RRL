// RRL/src/include/rhi/RHIBackend.hpp
#pragma once


#include "RRL/rhi/RHI.hpp"
#include "RRL/data/MeshData.hpp"
#include "RRL/data/MaterialData.hpp"


namespace rrl::rhi {

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
 * @brief Dispatch table for rendering backends.
 */
struct RHIBackend {
    RHIBackendType type { RHIBackendType::NONE };
    
    // Function pointers (defined for each backend type)
    
    // Lifecycle
    bool (*Initialize)(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window) { nullptr };
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


    // Materials
    MaterialHandle (*CreateMaterial)(entt::registry& registry, const data::MaterialData& material_data) { nullptr };
    void (*UpdateMaterial)(entt::registry& registry, MaterialHandle handle, const data::MaterialData& material_data) { nullptr };
    void (*DestroyMaterial)(entt::registry& registry, MaterialHandle handle) { nullptr };


    // Presentation
    // Reads rendered data from the RHI back to CPU RAM
    data::ImageData (*GetTargetImage)(entt::registry& registry, RenderTargetHandle handle) { nullptr };
    // Asks the backend to swap its TARGET_MAIN buffer to the attached window
    void (*Present)(entt::registry& registry) { nullptr };
    // Alert the backend that the used window (also headless dummy window) has been destroyed
    void (*OnWindowDestroyed)(entt::registry& registry, const RHIWindow* window) { nullptr };


    // Debugging
    void (*SetDebugFlag)(entt::registry& registry, RHIDebugFlag flag, bool enable) { nullptr };
    RHIDebugFlag (*GetActiveDebugFlags)(entt::registry& registry) { nullptr };

};


} // namespace rrl::rhi 

