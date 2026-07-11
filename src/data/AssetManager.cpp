// RRL/src/data/AssetManager.hpp

#include <entt/entt.hpp>

#include "RRL/data/AssetManager.hpp"
#include "RRL/data/AssetCache.hpp"
#include "RRL/data/AssetReferenceCounter.hpp"

#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MaterialData.hpp"
#include "RRL/data/MaterialComponents.hpp"


#include "RRL/rhi/RHI_Internal.hpp"


#include "RRL/DebugMacros.hpp"
#include <FLogging/FLogging.hpp>


namespace rrl::data {



// --- Hardware Resource Cleaners ----------------------------------
static void OnTextureRuntimeDestroyed(entt::registry& registry, entt::entity entity) {
    auto& runtime = registry.get<TextureRuntimeComponent>(entity);
    if (runtime.handle != rhi::TEXTURE_NULL) {
        rhi::DestroyTexture(registry, runtime.handle);
    }
}
static void OnMeshRuntimeDestroyed(entt::registry& registry, entt::entity entity) {
    auto& runtime = registry.get<MeshRuntimeComponent>(entity);
    if (runtime.handle != rhi::MESH_NULL) {
        rhi::DestroyMesh(registry, runtime.handle);
    }
}
static void OnMaterialRuntimeDestroyed(entt::registry& registry, entt::entity entity) {
    auto& runtime = registry.get<MaterialRuntimeComponent>(entity);
    if (runtime.handle != rhi::MATERIAL_NULL) {
        rhi::DestroyMaterial(registry, runtime.handle);
    }
}


// --- Asset GC Destructors ----------------------------------------
static void OnTextureLinkageDestroyed(entt::registry& registry, entt::entity entity) {
    auto& linkage = registry.get<TextureLinkage>(entity);
    DecrementAssetRef(registry, linkage.texture_asset);
}
static void OnMeshLinkageDestroyed(entt::registry& registry, entt::entity entity) {
    auto& linkage = registry.get<MeshLinkage>(entity);
    DecrementAssetRef(registry, linkage.mesh_asset);
    for (entt::entity mat : linkage.materials) {
        if (!registry.valid(mat)) continue;
        DecrementAssetRef(registry, mat);
    }
}
static void OnMaterialSourceDestroyed(entt::registry& registry, entt::entity entity) {
    auto& mat = registry.get<MaterialSourceComponent>(entity).data;
    // Cascade dereference counting down to the textures
    DecrementAssetRef(registry, mat.albedo_map);
    DecrementAssetRef(registry, mat.normal_map);
    DecrementAssetRef(registry, mat.metallic_roughness_map);
    DecrementAssetRef(registry, mat.emissive_map);
}



// --- Lifecycle ---------------------------------------------------
void InitializeAssetManager(entt::registry& registry) {
    registry.ctx().emplace<AssetCache>();
    
    // Hardware Resource Cleanups
    registry.on_destroy<TextureRuntimeComponent>().connect<&OnTextureRuntimeDestroyed>();
    registry.on_destroy<MeshRuntimeComponent>().connect<&OnMeshRuntimeDestroyed>();
    registry.on_destroy<MaterialRuntimeComponent>().connect<&OnMaterialRuntimeDestroyed>();

    // Asset Garbage Collection
    registry.on_destroy<TextureLinkage>().connect<&OnTextureLinkageDestroyed>();
    registry.on_destroy<MeshLinkage>().connect<&OnMeshLinkageDestroyed>();
    registry.on_destroy<MaterialSourceComponent>().connect<&OnMaterialSourceDestroyed>();
}
void SetAssetGCPolicy(entt::registry& registry, AssetGCPolicy policy) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    if (registry.ctx().contains<AssetCache>()) {
        registry.ctx().get<AssetCache>().gc_policy = policy;
    }
}
AssetGCPolicy GetAssetGCPolicy(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    if (registry.ctx().contains<AssetCache>()) {
        return registry.ctx().get<AssetCache>().gc_policy;
    }
    return AssetGCPolicy::CASCADE_DELETE;
}
void FreeUnusedAssets(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    bool assets_freed = true;
    
    // Keep sweeping until the dependency cascade fully resolves
    while (assets_freed) {
        assets_freed = false;
        std::vector<entt::entity> to_destroy;
        
        // Get all 0 referenced assets
        auto view = registry.view<AssetReferenceCounter>();
        for (auto entity : view) {
            if (view.get<AssetReferenceCounter>(entity).live_instances == 0) {
                to_destroy.push_back(entity);
            }
        }

        // Destroy them
        if (!to_destroy.empty()) {
            assets_freed = true;
            for (auto entity : to_destroy) {
                DestroyAsset(registry, entity);
            }
        }
    }
}
void DestroyAsset(entt::registry& registry, entt::entity asset_entity) {
    if (!registry.valid(asset_entity)) return;
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");

    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        for (auto it = cache.textures.begin(); it != cache.textures.end(); ) {
            if (it->second == asset_entity) it = cache.textures.erase(it);
            else ++it;
        }
        for (auto it = cache.meshes.begin(); it != cache.meshes.end(); ) {
            if (it->second == asset_entity) it = cache.meshes.erase(it);
            else ++it;
        }
        for (auto it = cache.materials.begin(); it != cache.materials.end(); ) {
            if (it->second == asset_entity) it = cache.materials.erase(it);
            else ++it;
        }
    }

    // Destroying the entity triggers the GC callbacks
    registry.destroy(asset_entity); 
}
void DestroyAllAssets(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    
    // Clear caches first
    if (registry.ctx().contains<AssetCache>()) {
        registry.ctx().get<AssetCache>().textures.clear();
        registry.ctx().get<AssetCache>().meshes.clear();
        registry.ctx().get<AssetCache>().materials.clear();
    }

    auto textures = registry.view<TextureSourceComponent>();
    registry.destroy(textures.begin(), textures.end());

    auto meshes = registry.view<MeshSourceComponent>();
    registry.destroy(meshes.begin(), meshes.end());

    auto materials = registry.view<MaterialSourceComponent>();
    registry.destroy(materials.begin(), materials.end());
}



// --- Textures ----------------------------------------------------
entt::entity GetCachedTexture(entt::registry& registry, const TextureID& texture_id) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        auto it = cache.textures.find(texture_id);
        if (it != cache.textures.end()) return it->second;
    }
    return entt::null;
}
entt::entity CreateTexture(entt::registry& registry, const TextureID& texture_id, ImageData&& image_data) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    RRL_ASSERT(!texture_id.empty(), "CreateTexture: texture_id cannot be empty!");

    if (registry.ctx().contains<AssetCache>() && !texture_id.empty()) {
        auto& cache = registry.ctx().get<AssetCache>();
        auto cached_texture = GetCachedTexture(registry, texture_id);
        if (cached_texture != entt::null) {
            LOG_WARN("CreateTexture: texture '{}' already exists on cache. Overriding it", texture_id);
            DestroyAsset(registry, cached_texture);
        }

        entt::entity tex_entity = registry.create();
        auto& source = registry.emplace<TextureSourceComponent>(tex_entity);
        source.image = std::make_shared<ImageData>(std::move(image_data)); 
        source.version = 1;
        cache.textures[texture_id] = tex_entity;
        return tex_entity;
    }
    return entt::null;
}
void UpdateTexture(entt::registry& registry, entt::entity texture_asset, ImageData&& image_data) {
    if (texture_asset == entt::null) {
        LOG_ERROR("UpdateTexture: Received null texture!");
        return;
    }
    RRL_ASSERT(registry.valid(texture_asset), "UpdateTexture: Invalid texture asset entity!");
    RRL_ASSERT(registry.all_of<TextureSourceComponent>(texture_asset), "UpdateTexture: Entity provided lacks a TextureSourceComponent!");
    
    // Validate that the incoming frame metadata matches the initialized asset allocations
    auto& source = registry.get<TextureSourceComponent>(texture_asset);
    RRL_ASSERT(source.image != nullptr, "UpdateTextureSource: Internal texture asset storage is null!");
    RRL_ASSERT(source.image->width == image_data.width, "UpdateTextureSource: Dimension mismatch (Width)!");
    RRL_ASSERT(source.image->height == image_data.height, "UpdateTextureSource: Dimension mismatch (Height)!");
    RRL_ASSERT(source.image->channels == image_data.channels, "UpdateTextureSource: Layout mismatch (Channels)!");
    RRL_ASSERT(source.image->data_type == image_data.data_type, "UpdateTextureSource: Type mismatch (DataType)!");

    source.image->data = std::move(image_data.data);
    source.version.fetch_add(1, std::memory_order_release);
}



// --- Meshes ------------------------------------------------------
entt::entity GetCachedMesh(entt::registry& registry, const MeshID& mesh_id) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        auto it = cache.meshes.find(mesh_id);
        if (it != cache.meshes.end()) return it->second;
    }
    return entt::null;
}
entt::entity CreateMesh(entt::registry& registry, const MeshID& mesh_id,MeshData&& mesh_data) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    RRL_ASSERT(!mesh_id.empty(), "CreateMesh: mesh_id cannot be empty!");

    if (registry.ctx().contains<AssetCache>() && !mesh_id.empty()) {
        auto& cache = registry.ctx().get<AssetCache>();
        auto cached_mesh = GetCachedMesh(registry, mesh_id);
        if (cached_mesh != entt::null) {
            LOG_WARN("CreateMesh: mesh '{}' already exists on cache. Overriding it", mesh_id);
            DestroyAsset(registry, cached_mesh);
        }
        
        entt::entity mesh_entity = registry.create();
        auto& source = registry.emplace<MeshSourceComponent>(mesh_entity);
        source.mesh = std::make_shared<MeshData>(std::move(mesh_data)); // Move the geometry vectors
        source.version = 1;
        cache.meshes[mesh_id] = mesh_entity;
        return mesh_entity;
    }
    return entt::null;
    
}
void UpdateMesh(entt::registry& registry, entt::entity mesh_asset, MeshData&& mesh_data) {
    if (mesh_asset == entt::null) return;
    RRL_ASSERT(registry.valid(mesh_asset), "UpdateMesh: Invalid mesh asset entity!");
    RRL_ASSERT(registry.all_of<MeshSourceComponent>(mesh_asset), "UpdateMesh: Entity lacks a MeshSourceComponent!");

    auto& source = registry.get<MeshSourceComponent>(mesh_asset);
    source.mesh = std::make_shared<MeshData>(std::move(mesh_data));
    source.version.fetch_add(1, std::memory_order_release);
}
void BindMesh(entt::registry& registry, entt::entity world_object, entt::entity mesh_asset, const std::vector<entt::entity>& materials, rhi::RHIRenderLayer layer) {
    RRL_ASSERT(registry.valid(world_object), "BindMesh: Invalid world object entity!");
    std::vector<entt::entity> resolved_materials = materials;

    // Resolve submesh materials
    if (mesh_asset != entt::null && registry.valid(mesh_asset) && registry.all_of<MeshSourceComponent>(mesh_asset)) {
        auto& source = registry.get<MeshSourceComponent>(mesh_asset);
        if (source.mesh) {
            size_t required_count = source.mesh->submeshes.empty() ? 1 : source.mesh->submeshes.size();
            size_t provided_count = resolved_materials.size();

            // Perfect match!. Do nothing.
            if (provided_count == required_count) {
            } 
            // Broadcast the single material to all slots
            else if (provided_count == 1 && required_count > 1) {
                resolved_materials.resize(required_count, resolved_materials[0]);
            } 
            // Mismatch. Either 0 materials provided, or a weird number like 2 materials for 5 submeshes
            else {
                if (provided_count > 0) {
                    LOG_WARN("BindMesh: Mesh has {} submeshes but {} materials were provided. Padding with nulls / truncating.", required_count, provided_count);
                }
                resolved_materials.resize(required_count, entt::null);
            }
        }
    }

    // Safely swap reference counts if overwriting an existing linkage
    if (auto* old_link = registry.try_get<MeshLinkage>(world_object)) {
        DecrementAssetRef(registry, old_link->mesh_asset);
        for (entt::entity mat : old_link->materials) DecrementAssetRef(registry, mat);
    }
    if (mesh_asset != entt::null) IncrementAssetRef(registry, mesh_asset);
    for (entt::entity mat : resolved_materials) {
        if (mat != entt::null) IncrementAssetRef(registry, mat);
    }
    
    registry.emplace_or_replace<MeshLinkage>(world_object, mesh_asset, resolved_materials, layer);
}
void SetMeshLayer(entt::registry& registry, entt::entity world_object, rhi::RHIRenderLayer layer) {
    RRL_ASSERT(registry.valid(world_object), "SetMeshLayer: Invalid world object entity!");
    RRL_ASSERT_HAS_COMPONENT(registry, world_object, MeshLinkage, "SetMeshLayer: Provided world object has not a MeshLinkage. Have you called BindMesh()?");
    if (auto* link = registry.try_get<MeshLinkage>(world_object)) {
        link->layer_mask = layer;
    }
}



// --- Materials ---------------------------------------------------
entt::entity GetCachedMaterial(entt::registry& registry, const MaterialID& material_id) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        auto it = cache.materials.find(material_id);
        if (it != cache.materials.end()) return it->second;
    }
    return entt::null;
}
entt::entity CreateMaterial(entt::registry& registry, const MaterialID& material_id, const MaterialData& mat_data) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    RRL_ASSERT(!material_id.empty(), "CreateMaterial: material_id cannot be empty!");
    
    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        auto cached_mat = GetCachedMaterial(registry, material_id);
        if (cached_mat != entt::null) {
            LOG_WARN("CreateMaterial: material '{}' already exists on cache. Overriding it", material_id);
            DestroyAsset(registry, cached_mat);
        }

        // Isolate incoming textures to route them securely through BindMaterialTexture
        MaterialData clean_data = mat_data;
        clean_data.albedo_map = entt::null;
        clean_data.normal_map = entt::null;
        clean_data.metallic_roughness_map = entt::null;
        clean_data.emissive_map = entt::null;

        entt::entity mat_entity = registry.create();
        auto& source = registry.emplace<MaterialSourceComponent>(mat_entity);
        source.data = clean_data;
        source.version = 1;
        
        cache.materials[material_id] = mat_entity;

        // Securely bind textures
        if (registry.valid(mat_data.albedo_map)) BindMaterialTexture(registry, mat_entity, mat_data.albedo_map, MaterialTextureType::ALBEDO);
        if (registry.valid(mat_data.normal_map)) BindMaterialTexture(registry, mat_entity, mat_data.normal_map, MaterialTextureType::NORMAL_MAP);
        if (registry.valid(mat_data.metallic_roughness_map)) BindMaterialTexture(registry, mat_entity, mat_data.metallic_roughness_map, MaterialTextureType::METALLIC_ROUGHNESS_MAP);
        if (registry.valid(mat_data.emissive_map)) BindMaterialTexture(registry, mat_entity, mat_data.emissive_map, MaterialTextureType::EMISSIVE_MAP);
        return mat_entity;
    }

    return entt::null;
}
void UpdateMaterial(entt::registry& registry, entt::entity material_asset, const MaterialData& mat_data) {
    RRL_ASSERT(registry.valid(material_asset), "UpdateMaterial: Invalid material asset!");
    RRL_ASSERT(registry.all_of<MaterialSourceComponent>(material_asset), "UpdateMaterial: Lacks MaterialSourceComponent!");

    // Preserve the old textures inside the patch to let the Bind API handle the swapping cleanly
    auto& source = registry.get<MaterialSourceComponent>(material_asset);
    auto& old_mat = source.data;
    MaterialData next_mat = mat_data;

    next_mat.albedo_map = old_mat.albedo_map;
    next_mat.normal_map = old_mat.normal_map;
    next_mat.metallic_roughness_map = old_mat.metallic_roughness_map;
    next_mat.emissive_map = old_mat.emissive_map;
    old_mat = next_mat;

    // Update the version so the RHI knows a material was updated
    source.version.fetch_add(1, std::memory_order_release);

    // Securely swap bindings
    if (old_mat.albedo_map != mat_data.albedo_map) BindMaterialTexture(registry, material_asset, mat_data.albedo_map, MaterialTextureType::ALBEDO);
    if (old_mat.normal_map != mat_data.normal_map) BindMaterialTexture(registry, material_asset, mat_data.normal_map, MaterialTextureType::NORMAL_MAP);
    if (old_mat.metallic_roughness_map != mat_data.metallic_roughness_map) BindMaterialTexture(registry, material_asset, mat_data.metallic_roughness_map, MaterialTextureType::METALLIC_ROUGHNESS_MAP);
    if (old_mat.emissive_map != mat_data.emissive_map) BindMaterialTexture(registry, material_asset, mat_data.emissive_map, MaterialTextureType::EMISSIVE_MAP);
}
void BindMaterialTexture(entt::registry& registry, entt::entity material_asset, entt::entity texture_asset, 
                 MaterialTextureType texture_type) {
    RRL_ASSERT(registry.valid(material_asset), "BindMaterialTexture: Invalid material asset!");
    if (texture_asset != entt::null) {
        RRL_ASSERT(registry.valid(texture_asset), "BindMaterialTexture: Invalid texture asset!");
        RRL_ASSERT(registry.all_of<TextureSourceComponent>(texture_asset), "BindMaterialTexture: Lacks TextureSourceComponent!");
    }

    if (registry.all_of<MaterialSourceComponent>(material_asset)) {
        auto& source = registry.get<MaterialSourceComponent>(material_asset);
        auto& mat = source.data;
        entt::entity* target_slot = nullptr;
        
        switch (texture_type) {
            case MaterialTextureType::ALBEDO: target_slot = &mat.albedo_map; break;
            case MaterialTextureType::NORMAL_MAP: target_slot = &mat.normal_map; break;
            case MaterialTextureType::METALLIC_ROUGHNESS_MAP: target_slot = &mat.metallic_roughness_map; break;
            case MaterialTextureType::EMISSIVE_MAP: target_slot = &mat.emissive_map; break;
        }

        if (target_slot && *target_slot != texture_asset) {
            DecrementAssetRef(registry, *target_slot);
            IncrementAssetRef(registry, texture_asset);
            *target_slot = texture_asset;
            
            // Update the version so the RHI knows a texture was swapped!
            source.version.fetch_add(1, std::memory_order_release);
        }
    }
}



// --- UI Assets ---------------------------------------------------
void BindUITexture(entt::registry& registry, entt::entity ui_object, entt::entity texture_asset,
                   float screen_x, float screen_y, float screen_w, float screen_h,
                   rhi::RHIRenderLayer layer 
) {
    RRL_ASSERT(registry.valid(ui_object), "BindUITexture: Invalid UI object!");
    if (texture_asset != entt::null) {
        RRL_ASSERT(registry.valid(texture_asset), "BindUITexture: Invalid texture asset!");
        RRL_ASSERT(registry.all_of<TextureSourceComponent>(texture_asset), "BindUITexture: Lacks TextureSourceComponent!");
    }
    
    if (auto* old_link = registry.try_get<TextureLinkage>(ui_object)) {
        if (old_link->texture_asset != texture_asset) {
            DecrementAssetRef(registry, old_link->texture_asset);
            IncrementAssetRef(registry, texture_asset);
        }
    } else {
        IncrementAssetRef(registry, texture_asset);
    }
    
    registry.emplace_or_replace<TextureLinkage>(ui_object, texture_asset, screen_x, screen_y, screen_w, screen_h, layer);
}

void UpdateUILayout(entt::registry& registry, entt::entity ui_object, 
                    float screen_x, float screen_y, 
                    float screen_w, float screen_h, 
                    rhi::RHIRenderLayer layer 
){
    RRL_ASSERT(registry.valid(ui_object), "UpdateUILayout: Invalid UI object!");
    RRL_ASSERT(registry.all_of<TextureLinkage>(ui_object), "UpdateUILayout: Lacks TextureLinkage!");

    auto& layout = registry.get<TextureLinkage>(ui_object);
    layout.screen_x = screen_x;
    layout.screen_y = screen_y;
    layout.screen_w = screen_w;
    layout.screen_h = screen_h;
    layout.layer_mask = layer;
}


// --- GB Interface ------------------------------------------------
void IncrementAssetRef(entt::registry& registry, entt::entity asset) {
    if (asset != entt::null && registry.valid(asset)) {
        registry.get_or_emplace<AssetReferenceCounter>(asset).live_instances++;
    }
}
void DecrementAssetRef(entt::registry& registry, entt::entity asset) {
    if (asset != entt::null && registry.valid(asset)) {
        if (auto* ref = registry.try_get<AssetReferenceCounter>(asset)) {
            if (ref->live_instances > 0) {
                if (--ref->live_instances == 0) {
                    
                    // Check the current policy
                    auto policy = AssetGCPolicy::CASCADE_DELETE;
                    if (registry.ctx().contains<AssetCache>()) {
                        policy = registry.ctx().get<AssetCache>().gc_policy;
                    }

                    // Only destroy if we are actively cascading
                    if (policy == AssetGCPolicy::CASCADE_DELETE) {
                        DestroyAsset(registry, asset); 
                    }
                }
            }
        }
    }
}

} // namespace rrl::data