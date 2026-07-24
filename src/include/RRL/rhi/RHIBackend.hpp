// RRL/src/include/rhi/RHIBackend.hpp
#pragma once

#include <entt/entt.hpp>

#include "RRL/rhi/RHITypes.hpp"

#include "RRL/asset/ImageAsset.hpp"
#include "RRL/asset/MeshAsset.hpp"
#include "RRL/asset/MaterialAsset.hpp"



namespace rrl::rhi {

// RHI texture handling (loaded/allocated textures)
using TextureHandle = uint32_t;
constexpr TextureHandle BACKEND_TEXTURE_NULL = 0xFFFFFFFF;      // Null handle

// RHI mesh handling (loaded/allocated meshes)
using MeshHandle = uint32_t;
constexpr MeshHandle BACKEND_MESH_NULL = 0xFFFFFFFF;            // Null handle

// RHI material handling (UBO tracking)
using MaterialHandle = uint32_t;
constexpr MaterialHandle BACKEND_MATERIAL_NULL = 0xFFFFFFFF;    // Null handle

// RHI rendering target ID (enable off-screen rendering support)
using RenderTargetHandle = uint32_t;
constexpr RenderTargetHandle BACKEND_TARGET_MAIN = 0;           // Handle 0 is reserved for the Screen Swapchain / Main Target
constexpr RenderTargetHandle BACKEND_TARGET_NULL = 0xFFFFFFFF;  // Null handle


/**
 * @brief Internal descriptor translated by the RHI for the backend.
 * Contains physical texture handles instead of Virtual ResourceIDs.
 * @note see include/rhi/RHITypes.hpp RenderTargetDescriptor for 
 * the public interface.
 */
struct PhysicalRenderTargetDescriptor {
    uint32_t width = 0;
    uint32_t height = 0;
    
    std::vector<rrl::rhi::TextureHandle> color_attachments;
    rrl::rhi::TextureHandle depth_stencil_attachment = rrl::rhi::BACKEND_TEXTURE_NULL;
    
    bool is_texture_array = false;
    uint32_t array_idx = 0; 
};


/**
 * @brief Backend cache. This enables runtime, backend specific virtual resource descriptors mapping.
 */
struct RHIBackendCache {
    
    // Bidirectional Resource Mappings
    std::unordered_map<entt::id_type, TextureHandle>      textures;
    std::unordered_map<TextureHandle, entt::id_type>      rev_textures;

    std::unordered_map<entt::id_type, MeshHandle>         meshes;
    std::unordered_map<MeshHandle, entt::id_type>         rev_meshes;

    std::unordered_map<entt::id_type, MaterialHandle>     materials;
    std::unordered_map<MaterialHandle, entt::id_type>     rev_materials;

    std::unordered_map<entt::id_type, RenderTargetHandle> targets;
    std::unordered_map<RenderTargetHandle, entt::id_type> rev_targets;


    // Textures
    inline void RegisterTexture(ResourceID id, TextureHandle handle) {
        textures[id.id] = handle;
        rev_textures[handle] = id.id;
    }
    inline void UnregisterTexture(ResourceID id) {
        if (auto it = textures.find(id.id); it != textures.end()) {
            rev_textures.erase(it->second);
            textures.erase(it);
        }
    }
    inline TextureHandle GetPhysicalTexture(ResourceID id) const {
        if (auto it = textures.find(id.id); it != textures.end()) return it->second;
        return BACKEND_TEXTURE_NULL;
    }
    inline ResourceID GetVirtualTexture(TextureHandle handle) const {
        if (auto it = rev_textures.find(handle); it != rev_textures.end()) return ResourceID{it->second};
        return RESOURCE_NULL;
    }

    // Meshes
    inline void RegisterMesh(ResourceID id, MeshHandle handle) {
        meshes[id.id] = handle;
        rev_meshes[handle] = id.id;
    }
    inline void UnregisterMesh(ResourceID id) {
        if (auto it = meshes.find(id.id); it != meshes.end()) {
            rev_meshes.erase(it->second);
            meshes.erase(it);
        }
    }
    inline MeshHandle GetPhysicalMesh(ResourceID id) const {
        if (auto it = meshes.find(id.id); it != meshes.end()) return it->second;
        return BACKEND_MESH_NULL;
    }
    inline ResourceID GetVirtualMesh(MeshHandle handle) const {
        if (auto it = rev_meshes.find(handle); it != rev_meshes.end()) return ResourceID{it->second};
        return RESOURCE_NULL;
    }

    // Materials
    inline void RegisterMaterial(ResourceID id, MaterialHandle handle) {
        materials[id.id] = handle;
        rev_materials[handle] = id.id;
    }
    inline void UnregisterMaterial(ResourceID id) {
        if (auto it = materials.find(id.id); it != materials.end()) {
            rev_materials.erase(it->second);
            materials.erase(it);
        }
    }
    inline MaterialHandle GetPhysicalMaterial(ResourceID id) const {
        if (auto it = materials.find(id.id); it != materials.end()) return it->second;
        return BACKEND_MATERIAL_NULL;
    }
    inline ResourceID GetVirtualMaterial(MaterialHandle handle) const {
        if (auto it = rev_materials.find(handle); it != rev_materials.end()) return ResourceID{it->second};
        return RESOURCE_NULL;
    }

    // Targets (FBOs)
    inline void RegisterTarget(ResourceID id, RenderTargetHandle handle) {
        targets[id.id] = handle;
        rev_targets[handle] = id.id;
    }
    inline void UnregisterTarget(ResourceID id) {
        if (auto it = targets.find(id.id); it != targets.end()) {
            rev_targets.erase(it->second);
            targets.erase(it);
        }
    }
    inline RenderTargetHandle GetPhysicalTarget(ResourceID id) const {
        if (id == TARGET_MAIN) return BACKEND_TARGET_MAIN; // Special hardware target
        if (auto it = targets.find(id.id); it != targets.end()) return it->second;
        return BACKEND_TARGET_NULL;
    }
    inline ResourceID GetVirtualTarget(RenderTargetHandle handle) const {
        if (handle == BACKEND_TARGET_MAIN) return TARGET_MAIN; // Special hardware target
        if (auto it = rev_targets.find(handle); it != rev_targets.end()) return ResourceID{it->second};
        return RESOURCE_NULL;
    }
};




/**
 * @brief Dispatch table for rendering backends.
 */
struct RHIBackend {
    RHIBackendType type { RHIBackendType::NONE };
    RHIBackendCache cache;
    
    // Function pointers (defined for each backend type)
    
    // Lifecycle
    bool (*Initialize)(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window) { nullptr };
    void (*Shutdown)(entt::registry& registry) { nullptr };
    void (*RenderFrame)(entt::registry& registry) { nullptr };
    
    // Target FBOs
    TextureHandle (*CreateRenderTexture)(entt::registry& registry, uint32_t width, uint32_t height, rrl::asset::ImageAssetType data_type, rrl::asset::ImageChannelLayout channels, bool is_depth, uint32_t array_layers) { nullptr };
    RenderTargetHandle (*CreateRenderTarget)(entt::registry& registry, const PhysicalRenderTargetDescriptor& desc) { nullptr };
    void (*DestroyRenderTarget)(entt::registry& registry, RenderTargetHandle handle) { nullptr };

    // Textures
    TextureHandle (*CreateTexture)(entt::registry& registry, const rrl::asset::ImageAsset& image_data) { nullptr };
    void (*UpdateTexture)(entt::registry& registry, TextureHandle handle, const rrl::asset::ImageAsset& image_data) { nullptr };
    void (*DestroyTexture)(entt::registry& registry, TextureHandle handle) { nullptr };
    
    // Meshes
    MeshHandle (*CreateMesh)(entt::registry& registry, const rrl::asset::MeshAsset& mesh_data) { nullptr };
    void (*UpdateMesh)(entt::registry& registry, MeshHandle handle, const rrl::asset::MeshAsset& mesh_data) { nullptr };
    void (*DestroyMesh)(entt::registry& registry, MeshHandle handle) { nullptr };


    // Materials
    MaterialHandle (*CreateMaterial)(entt::registry& registry, const rrl::asset::MaterialAsset& material_data) { nullptr };
    void (*UpdateMaterial)(entt::registry& registry, MaterialHandle handle, const rrl::asset::MaterialAsset& material_data) { nullptr };
    void (*DestroyMaterial)(entt::registry& registry, MaterialHandle handle) { nullptr };


    // Presentation
    // Reads rendered data from the RHI back to CPU RAM
    rrl::asset::ImageAsset (*GetTargetImage)(entt::registry& registry, RenderTargetHandle handle) { nullptr };
    // Asks the backend to swap its TARGET_MAIN buffer to the attached window
    void (*Present)(entt::registry& registry) { nullptr };
    // Alert the backend that the used window (also headless dummy window) has been destroyed
    void (*OnWindowDestroyed)(entt::registry& registry, const RHIWindow* window) { nullptr };


    // Debugging
    void (*SetDebugFlag)(entt::registry& registry, RHIDebugFlag flag, bool enable) { nullptr };
    RHIDebugFlag (*GetActiveDebugFlags)(entt::registry& registry) { nullptr };

};


} // namespace rrl::rhi 

