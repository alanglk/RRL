// RRL/src/rhi/RHI.cpp

#include "RRL/rhi/RHI.hpp"
#include "RRL/asset/ImageAsset.hpp"
#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/rhi/RHIBackendManager.hpp"

#include "RRL/asset/AssetSynchronizationSystems.hpp"
#include "RRL/scene/SceneSynchronizationSystems.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


// --- Backend Factories -------------------------------------------
// Forward declare the compile-time available Backend factories
namespace rrl::rhi::software { RHIBackend CreateSoftwareBackend(); }
#ifdef RRL_BUILD_BACKEND_OPENGL
namespace rrl::rhi::opengl   { RHIBackend CreateOpenGLBackend(); }
#endif


// --- Window Factories --------------------------------------------
// Forward declare the compile-time available Window factories
#ifdef RRL_BUILD_WINDOW_OPENCV 
namespace rrl::rhi::window::opencv {
    bool Initialize(rrl::rhi::RHIWindow& window, const char* title, uint32_t w, uint32_t h);
    bool PollEvents(rrl::rhi::RHIWindow& window);
    void Shutdown(rrl::rhi::RHIWindow& window);
}
#endif
#ifdef RRL_BUILD_WINDOW_GLFW 
namespace rrl::rhi::window::glfw {
    bool Initialize(rrl::rhi::RHIWindow& window, const char* title, uint32_t w, uint32_t h);
    bool PollEvents(rrl::rhi::RHIWindow& window);
    void Shutdown(rrl::rhi::RHIWindow& window);
}
#endif



namespace rrl::rhi {



// --- Window ------------------------------------------------------
RHIWindow CreateWindow(RHIWindowType window_type) {
    RHIWindow win{};
    win.type = window_type;
    return win;
}
bool InitializeWindow(RHIWindow& window, const char* title, uint32_t w, uint32_t h) {
    switch (window.type) {
        case RHIWindowType::HEADLESS:
            window.width = w;
            window.height = h;
            window.native_handle = nullptr;
            return true;

        case RHIWindowType::OPENCV:
            #ifdef RRL_BUILD_WINDOW_OPENCV
            return window::opencv::Initialize(window, title, w, h);
            #else
            LOG_ERROR("InitializeWindow: OpenCV support not compiled in this build.");
            return false;
            #endif

        case RHIWindowType::GLFW:
            #ifdef RRL_BUILD_WINDOW_GLFW
            return window::glfw::Initialize(window, title, w, h);
            #else
            LOG_ERROR("InitializeWindow: GLFW + OpenGL support not compiled in this build.");
            return false;
            #endif
    }
    return false;
}
bool PollWindowEvents(RHIWindow& window) {
    if (window.type != RHIWindowType::HEADLESS) {
        RRL_ASSERT(window.native_handle != nullptr, "PollWindowEvents: Received a non initialized window!");
    }

    switch (window.type) {
        case RHIWindowType::HEADLESS:
            return true;

        case RHIWindowType::OPENCV:
            #ifdef RRL_BUILD_WINDOW_OPENCV
            return window::opencv::PollEvents(window);
            #else
            return false;
            #endif

        case RHIWindowType::GLFW:
            #ifdef RRL_BUILD_WINDOW_GLFW
            return window::glfw::PollEvents(window);
            #else
            return false;
            #endif
    }
    return false;
}
void DestroyWindow(entt::registry& registry, RHIWindow& window) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.type != RHIBackendType::NONE && backend.OnWindowDestroyed != nullptr) {
        backend.OnWindowDestroyed(registry, &window);
    }

    switch (window.type) {
        case RHIWindowType::HEADLESS:
            break;

        case RHIWindowType::OPENCV:
            #ifdef RRL_BUILD_WINDOW_OPENCV
            window::opencv::Shutdown(window);
            #endif
            break;

        case RHIWindowType::GLFW:
            #ifdef RRL_BUILD_WINDOW_GLFW
            window::glfw::Shutdown(window);
            #endif
            break;
    }

    window.native_handle = nullptr;
    window.width = 0;
    window.height = 0;
}



// --- Backend -----------------------------------------------------
bool LoadBackend(RHIBackendType target_backend, entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.type == target_backend) {
        return true; // Already loaded
    }

    // TODO: Implement hot-swapping logic (extract assets -> shutdown -> switch -> push assets)

    switch (target_backend) {
        case RHIBackendType::SOFTWARE:
            RHIBackendManager::Instance().SetBackend(rrl::rhi::software::CreateSoftwareBackend());
            return true;
            
        case RHIBackendType::OPENGL: {
            #ifdef RRL_BUILD_BACKEND_OPENGL
            RHIBackendManager::Instance().SetBackend(rrl::rhi::opengl::CreateOpenGLBackend());
            return true;
            #else
            LOG_ERROR("Loadbackend: OpenGL support not compiled in this build.");
            return false;
            #endif
            
        }
            
        default:
            LOG_ERROR("LoadBackend failed. Unknown or unavailable requested RHI backend.");
            return false;
    }
}
RHIBackendType GetCurrentBackendType() {
    return RHIBackendManager::Instance().GetBackend().type;
}



// --- Lifecycle ---------------------------------------------------
bool Initialize(entt::registry& registry, const RHIWindow* window) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.Initialize != nullptr, "RHI Initialize called but no backend is loaded!");
    RRL_ASSERT(window != nullptr, "RHI Initialize called but no window is provided!");
    return backend.Initialize(registry, window->width, window->height, window);
}
bool Initialize(entt::registry& registry, uint32_t render_width, uint32_t render_height, const RHIWindow* window) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.Initialize != nullptr, "RHI Initialize called but no backend is loaded!");
    RRL_ASSERT(window != nullptr, "RHI Initialize called but no window is provided!");
    return backend.Initialize(registry, render_width, render_height, window);
}
void Shutdown(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.Shutdown != nullptr) {
        backend.Shutdown(registry);
    }
    RHIBackendManager::Instance().Reset(); // Reset backend to NONE
}
void SyncResources(entt::registry& registry) {
    rrl::asset::SyncTexturesToRHI(registry);
    rrl::asset::SyncMeshesToRHI(registry);
    rrl::asset::SyncMaterialsToRHI(registry);
    rrl::scene::SyncSceneToRHI(registry);
}
void RenderFrame(entt::registry& registry) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.RenderFrame != nullptr, "RHI RenderFrame called but no backend is loaded!");

    SyncResources(registry);
    backend.RenderFrame(registry);
    Present(registry,
            backend.cache.presentation_state.id, 
            backend.cache.presentation_state.semantic, 
            backend.cache.presentation_state.array_index);
}



// --- Render Targets (FBOs) ---------------------------------------
void AllocateRenderTargetTexture(entt::registry& registry, ResourceID id, uint32_t width, uint32_t height, 
                           rrl::asset::ImageAssetType data_type, rrl::asset::ImageChannelLayout channels, 
                           bool is_depth, uint32_t array_layers) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateRenderTarget != nullptr, "RHI AllocateRenderTargetTexture called but no backend is loaded!");
    
    // Check if it already exists
    if (backend.cache.GetPhysicalTexture(id) != BACKEND_TEXTURE_NULL) {
        LOG_WARN("Texture '{}' is already allocated.", id.id);
        return;
    }

    TextureHandle tex_handle = backend.CreateRenderTexture(
        registry, width, height, data_type, channels, is_depth, array_layers
    );
    
    if (tex_handle != BACKEND_TEXTURE_NULL) {
        backend.cache.RegisterTexture(id, tex_handle, true, {width, height, data_type, channels, array_layers});
        LOG_INFO("[RHI] Allocated explicit render texture '{}' (Layers: {})", id.id, array_layers);
    }

}
void CreateRenderTarget(entt::registry& registry, ResourceID id, const rrl::rhi::RHIRenderTargetDescriptor& desc) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateRenderTarget != nullptr, "RHI CreateRenderTarget called but no backend is loaded!");
    rrl::rhi::PhysicalRenderTargetDescriptor phys_desc;
    phys_desc.width  = desc.width;
    phys_desc.height = desc.height;
    
    // Safety checks
    // Dimensions
    if (desc.width == 0 || desc.height == 0) {
        LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Dimensions cannot be zero ({}x{}).", id.id, desc.width, desc.height);
        return;
    }
    // Maximum Color Attachments
    constexpr size_t MAX_COLOR_ATTACHMENTS = 8;
    if (desc.color_attachments.size() > MAX_COLOR_ATTACHMENTS) {
        LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Requested {} color attachments, exceeding the maximum of {}.", id.id, desc.color_attachments.size(), MAX_COLOR_ATTACHMENTS);
        return;
    }
    // No duplicate color attachments or conflicting semantics
    for (size_t i = 0; i < desc.color_attachments.size(); ++i) {
        for (size_t j = i + 1; j < desc.color_attachments.size(); ++j) {
            // Check for duplicated physical textures
            if (desc.color_attachments[i].id == desc.color_attachments[j].id) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Duplicate color attachment ID '{}' detected.", id.id, desc.color_attachments[i].id.id);
                return;
            }
            // Check for duplicated semantics (prevents two textures fighting for layout(location = X))
            if (desc.color_attachments[i].semantic == desc.color_attachments[j].semantic) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Conflicting semantic detected. You cannot map multiple attachments to the same shader layout location.", id.id);
                return;
            }
        }
    }
    // Empty color attachments
    if (desc.color_attachments.empty()) {
        LOG_WARN("[RHI] Note on RenderTarget '{}': RHIRenderTargetDescriptor has 0 color attachments. This is valid for depth-only passes (e.g., shadow mapping), but ensure this is intentional.", id.id);
    }
    // Texture Array, Pre-allocation, and Semantic Validation
    for (const rrl::rhi::RHIRenderAttachmentDescriptor& val_desc : desc.color_attachments) {
        
        // Array Validation
        if (!val_desc.is_texture_array && val_desc.array_idx > 0) {
            LOG_ERROR("[RHI] Failed to create RenderTarget '{}': array_idx is {} but is_texture_array is false.", id.id, val_desc.array_idx);
            return;
        }
        if (val_desc.is_texture_array) {
            // Standard APIs (OpenGL 4.x / Vulkan) guarantee at least 2048 layers for texture arrays.
            constexpr uint32_t MAX_TEXTURE_ARRAY_LAYERS = 2048; 
            if (val_desc.array_idx >= MAX_TEXTURE_ARRAY_LAYERS) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': array_idx {} exceeds maximum texture array bounds ({}).", 
                          id.id, val_desc.array_idx, MAX_TEXTURE_ARRAY_LAYERS);
                return;
            }
        }

        // Pre-allocated Texture Validation
        TextureHandle existing_handle = backend.cache.GetPhysicalTexture(val_desc.id);
        if (existing_handle != BACKEND_TEXTURE_NULL) {

            // Validate it is a render texture
            if(!backend.cache.IsRenderTexture(val_desc.id)){
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Attached texture '{}' is not a render texture.", id.id, val_desc.id.id);
                return;
            }
            const rrl::rhi::RHIRenderTextureMetadata& meta = backend.cache.GetTextureMetadata(val_desc.id);
            
            // Validate Dimensions
            if (meta.width != desc.width || meta.height != desc.height) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Dimension mismatch! FBO is {}x{}, but attached texture '{}' is {}x{}.", 
                          id.id, desc.width, desc.height, val_desc.id.id, meta.width, meta.height);
                return;
            }
            // Validate Layout (Format)
            if (meta.data_type != val_desc.data_type || meta.channels != val_desc.channels) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Format mismatch! Attachment descriptor requests data_type: {} / channels: {}, but pre-allocated texture '{}' is data_type: {} / channels: {}.", 
                          id.id, static_cast<int>(val_desc.data_type), static_cast<int>(val_desc.channels), 
                          val_desc.id.id, static_cast<int>(meta.data_type), static_cast<int>(meta.channels));
                return;
            }
            // Validate Array Layers
            if (val_desc.is_texture_array && val_desc.array_idx >= meta.array_layers) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Array index {} is out of bounds for texture '{}' which only has {} layers.", 
                          id.id, val_desc.array_idx, val_desc.id.id, meta.array_layers);
                return;
            }
        }

        // Semantic Compatibility Validation
        switch (val_desc.semantic) {
            case rrl::rhi::RHIRenderAttachmentSemantic::DEPTH:
                if (val_desc.channels != rrl::asset::ImageChannelLayout::CH_1 || 
                    val_desc.data_type != rrl::asset::ImageAssetType::FLOAT32) {
                    LOG_WARN("[RHI] Note on RenderTarget '{}': [Attachment '{}']: DEPTH semantic expects a CH_1 FLOAT32 texture for linearized metric depth. Binding a different format may cause severe MRT corruption or clamp values.", id.id, val_desc.id.id);
                }
                break;
                
            case rrl::rhi::RHIRenderAttachmentSemantic::NORMAL_SURFACE:
                if (val_desc.channels == rrl::asset::ImageChannelLayout::CH_1 || 
                    val_desc.channels == rrl::asset::ImageChannelLayout::CH_2) {
                    LOG_WARN("[RHI] Note on RenderTarget '{}': [Attachment '{}']: NORMAL_SURFACE semantic expects CH_3 or CH_4 to store 3D vectors. Truncated channels detected.", id.id, val_desc.id.id);
                }
                break;
                
            case rrl::rhi::RHIRenderAttachmentSemantic::PANOPTIC_SEGMENTATION:
                if (val_desc.data_type == rrl::asset::ImageAssetType::FLOAT32) {
                    LOG_WARN("[RHI] Note on RenderTarget '{}': [Attachment '{}']: PANOPTIC_SEGMENTATION semantic requires integer types (UINT8 or UINT16) for exact IDs. Using FLOAT32 risks precision loss and interpolation bleeding on masks.", id.id, val_desc.id.id);
                }
                break;
                
            case rrl::rhi::RHIRenderAttachmentSemantic::COLOR:
                if (val_desc.channels == rrl::asset::ImageChannelLayout::CH_1 || 
                    val_desc.channels == rrl::asset::ImageChannelLayout::CH_2) {
                    LOG_WARN("[RHI] Note on RenderTarget '{}': [Attachment '{}']: COLOR semantic normally expects CH_3 or CH_4. Verify if a grayscale/2-channel color output is intentional.", id.id, val_desc.id.id);
                }
                break;
                
            default:
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Invalid or unmapped RHIRenderAttachmentSemantic. Developers may have forgotten to update this checking!");
                return;
        }
    }



    // Actual creation
    // Auto-allocate or Resolve Color Attachments
    for (const rrl::rhi::RHIRenderAttachmentDescriptor& color_desc : desc.color_attachments) {
        TextureHandle tex_handle = backend.cache.GetPhysicalTexture(color_desc.id);
        
        if (tex_handle == BACKEND_TEXTURE_NULL) {
            // If the user wants an index of an array, it MUST already exist.
            if (color_desc.is_texture_array) {
                LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Cannot lazily allocate Texture Array slice for target '{}'. You must call AllocateRenderTexture first.", id.id, color_desc.id.id);
                return;
            }

            // Lazy Allocation for simple 2D textures
            tex_handle = backend.CreateRenderTexture(registry, desc.width, desc.height, 
                                                     color_desc.data_type, 
                                                     color_desc.channels, false, 1);
            backend.cache.RegisterTexture(color_desc.id, tex_handle, true, {desc.width, desc.height, color_desc.data_type, color_desc.channels, 1});
        }

        // Push attachment with its specific routing logic
        phys_desc.color_attachments.push_back({
            .handle = tex_handle,
            .is_texture_array = color_desc.is_texture_array,
            .array_idx = color_desc.array_idx,
            .layout_location = MapSemanticToLayoutLocation(color_desc.semantic)
        });
    }

    // Dispatch physical creation to the backend
    RenderTargetHandle physical_handle = backend.CreateRenderTarget(registry, phys_desc);
    if (physical_handle != BACKEND_TARGET_NULL) {
        backend.cache.RegisterTarget(id, physical_handle);
    } else {
        LOG_ERROR("[RHI] Failed to create RenderTarget '{}': Backend could not create the RenderTarget.", id.id);
    }
}
void DestroyRenderTarget(entt::registry& registry, ResourceID id) {
    if (id == TARGET_MAIN) return; // Cannot destroy the main swapchain

    auto& backend = RHIBackendManager::Instance().GetBackend();
    RenderTargetHandle physical_handle = backend.cache.GetPhysicalTarget(id);
    
    if (physical_handle != BACKEND_TARGET_NULL && backend.DestroyRenderTarget != nullptr) {
        backend.DestroyRenderTarget(registry, physical_handle);
        backend.cache.UnregisterTarget(id);
    }
}
void DestroyRenderTargetTexture(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    
    // Ensure it actually exists and is a Render Texture
    if (!backend.cache.IsRenderTexture(id)) {
        LOG_ERROR("RHI DestroyRenderTargetTexture: '{}' is either not loaded, or is a Texture Asset. DestroyRenderTargetTexture can ONLY destroy explicitly allocated render targets.", id.id);
        return;
    }

    TextureHandle physical_handle = backend.cache.GetPhysicalTexture(id);
    if (physical_handle != BACKEND_TEXTURE_NULL && backend.DestroyTexture != nullptr) {
        backend.DestroyTexture(registry, physical_handle); 
        backend.cache.UnregisterTexture(id);
    } else {
        LOG_WARN("Attempted to destroy invalid or already destroyed render target texture: '{}'", id.id);
    }
}

// --- Textures ----------------------------------------------------
TextureHandle CreateTexture(entt::registry& registry, ResourceID id, const rrl::asset::ImageAsset& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateTexture != nullptr, "RHI CreateTexture called but no backend is loaded!");
    RRL_ASSERT(image_data.IsValid(), "RHI CreateTexture called but invalid or not populated image was provided");
    
    TextureHandle physical_handle = backend.CreateTexture(registry, image_data);
    if (physical_handle != BACKEND_TEXTURE_NULL) {
        backend.cache.RegisterTexture(id, physical_handle);
    }
    return physical_handle;
}
void UpdateTexture(entt::registry& registry, TextureHandle handle, const rrl::asset::ImageAsset& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateTexture != nullptr, "RHI UpdateTexture called but no backend is loaded!");
    RRL_ASSERT(image_data.IsValid(), "RHI UpdateTexture called but invalid image provided");
    
    if (handle != BACKEND_TEXTURE_NULL) {
        backend.UpdateTexture(registry, handle, image_data);
    }
}
void UpdateTexture(entt::registry& registry, ResourceID id, const rrl::asset::ImageAsset& image_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateTexture != nullptr, "RHI UpdateTexture called but no backend is loaded!");
    RRL_ASSERT(image_data.IsValid(), "RHI UpdateTexture called but invalid or not populated image was provided");
    
    TextureHandle physical_handle = backend.cache.GetPhysicalTexture(id);
    if (physical_handle != BACKEND_TEXTURE_NULL) {
        backend.UpdateTexture(registry, physical_handle, image_data);
    } else {
        LOG_WARN("Attempted to update a texture that doesn't exist on the GPU (ID: {})", id.id);
    }
}
void DestroyTexture(entt::registry& registry, TextureHandle handle) {
    if (handle == BACKEND_TEXTURE_NULL) return;

    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyTexture != nullptr) {
        backend.DestroyTexture(registry, handle);
        
        ResourceID mapped_id = backend.cache.GetVirtualTexture(handle);
        if (mapped_id != RESOURCE_NULL) {
            backend.cache.UnregisterTexture(mapped_id);
        }
    }
}
void DestroyTexture(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    TextureHandle physical_handle = backend.cache.GetPhysicalTexture(id);
    
    if (physical_handle != BACKEND_TEXTURE_NULL && backend.DestroyTexture != nullptr) {
        backend.DestroyTexture(registry, physical_handle);
        backend.cache.UnregisterTexture(id);
    }
}


// --- Meshes ------------------------------------------------------
MeshHandle CreateMesh(entt::registry& registry, ResourceID id, const rrl::asset::MeshAsset& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateMesh != nullptr, "RHI CreateMesh called but no backend is loaded!");
    
    MeshHandle physical_handle = backend.CreateMesh(registry, mesh_data);
    if (physical_handle != BACKEND_MESH_NULL) {
        backend.cache.RegisterMesh(id, physical_handle);
    }
    return physical_handle;
}
void UpdateMesh(entt::registry& registry, MeshHandle handle, const rrl::asset::MeshAsset& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMesh != nullptr, "RHI UpdateMesh called but no backend is loaded!");
    
    if (handle != BACKEND_MESH_NULL) {
        backend.UpdateMesh(registry, handle, mesh_data);
    }
}
void UpdateMesh(entt::registry& registry, ResourceID id, const rrl::asset::MeshAsset& mesh_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMesh != nullptr, "RHI UpdateMesh called but no backend is loaded!");
    
    MeshHandle physical_handle = backend.cache.GetPhysicalMesh(id);
    if (physical_handle != BACKEND_MESH_NULL) {
        backend.UpdateMesh(registry, physical_handle, mesh_data);
    } else {
        LOG_WARN("Attempted to update a virtual mesh that doesn't exist on the GPU (ID: {})", id.id);
    }
}
void DestroyMesh(entt::registry& registry, MeshHandle handle) {
    if (handle == BACKEND_MESH_NULL) return;

    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyMesh != nullptr) {
        backend.DestroyMesh(registry, handle);
        
        ResourceID mapped_id = backend.cache.GetVirtualMesh(handle);
        if (mapped_id != RESOURCE_NULL) {
            backend.cache.UnregisterMesh(mapped_id);
        }
    }
}
void DestroyMesh(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    MeshHandle physical_handle = backend.cache.GetPhysicalMesh(id);
    
    if (physical_handle != BACKEND_MESH_NULL && backend.DestroyMesh != nullptr) {
        backend.DestroyMesh(registry, physical_handle);
        backend.cache.UnregisterMesh(id);
    }
}


// --- Materials ---------------------------------------------------
MaterialHandle CreateMaterial(entt::registry& registry, ResourceID id, const rrl::asset::MaterialAsset& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.CreateMaterial != nullptr, "RHI CreateMaterial called but no backend is loaded!");
    
    MaterialHandle physical_handle = backend.CreateMaterial(registry, material_data);
    if (physical_handle != BACKEND_MATERIAL_NULL) {
        backend.cache.RegisterMaterial(id, physical_handle);
    }
    return physical_handle;
}
void UpdateMaterial(entt::registry& registry, MaterialHandle handle, const rrl::asset::MaterialAsset& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMaterial != nullptr, "RHI UpdateMaterial called but no backend is loaded!");

    if (handle != BACKEND_MATERIAL_NULL) {
        backend.UpdateMaterial(registry, handle, material_data);
    }
}
void UpdateMaterial(entt::registry& registry, ResourceID id, const rrl::asset::MaterialAsset& material_data) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.UpdateMaterial != nullptr, "RHI UpdateMaterial called but no backend is loaded!");

    MaterialHandle physical_handle = backend.cache.GetPhysicalMaterial(id);
    if (physical_handle != BACKEND_MATERIAL_NULL) {
        backend.UpdateMaterial(registry, physical_handle, material_data);
    }
}
void DestroyMaterial(entt::registry& registry, MaterialHandle handle) {
    if (handle == BACKEND_MATERIAL_NULL) return;

    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.DestroyMaterial != nullptr) {
        backend.DestroyMaterial(registry, handle);
        
        ResourceID mapped_id = backend.cache.GetVirtualMaterial(handle);
        if (mapped_id != RESOURCE_NULL) {
            backend.cache.UnregisterMaterial(mapped_id);
        }
    }
}
void DestroyMaterial(entt::registry& registry, ResourceID id) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    
    MaterialHandle physical_handle = backend.cache.GetPhysicalMaterial(id);
    if (physical_handle != BACKEND_MATERIAL_NULL && backend.DestroyMaterial != nullptr) {
        backend.DestroyMaterial(registry, physical_handle);
        backend.cache.UnregisterMaterial(id);
    }else {
        LOG_WARN("Attempted to update a virtual material that doesn't exist on the GPU (ID: {})", id.id);
    }
}


// --- Environment -------------------------------------------------
void SetEnvironment(entt::registry& registry, const PhysicalEnvironmentDescriptor& env_desc) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    if (backend.SetEnvironment != nullptr) {
        backend.SetEnvironment(registry, env_desc);
    } else {
        LOG_WARN("RHI SetEnvironment called, but the active backend does not support environment rendering.");
    }
}



// --- Retrieve Rendered Data --------------------------------------
rrl::asset::ImageAsset GetTargetImage(entt::registry& registry, ResourceID id, RHIRenderAttachmentSemantic semantic, uint32_t array_index) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.GetTargetImage != nullptr, "RHI GetTargetImage called but no backend is loaded!");

    RenderTargetHandle physical_handle = backend.cache.GetPhysicalTarget(id);
    if (physical_handle == BACKEND_TARGET_NULL) return rrl::asset::ImageAsset{}; 

    // Return the backend target image
    return backend.GetTargetImage(registry, physical_handle, semantic, array_index);
}
void SetPresentationTarget(entt::registry& registry, ResourceID id, RHIRenderAttachmentSemantic semantic, uint32_t array_index) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.GetTargetImage != nullptr, "RHI GetTargetImage called but no backend is loaded!");
    backend.cache.presentation_state.id             = id;
    backend.cache.presentation_state.semantic       = semantic;
    backend.cache.presentation_state.array_index    = array_index;
}
void Present(entt::registry& registry, ResourceID id, RHIRenderAttachmentSemantic semantic, uint32_t array_index) {
    auto& backend = RHIBackendManager::Instance().GetBackend();
    RRL_ASSERT(backend.GetTargetImage != nullptr, "RHI GetTargetImage called but no backend is loaded!");

    RenderTargetHandle physical_handle = backend.cache.GetPhysicalTarget(id);
    if (physical_handle == BACKEND_TARGET_NULL) return;

    backend.Present(registry, physical_handle, semantic, array_index);
}



} // namespace rrl::rhi

