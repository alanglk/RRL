// RRL/src/data/AssetManager.hpp

#include "RRL/data/AssetManager.hpp"
#include "RRL/data/AssetReferenceCounter.hpp"
#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MeshData.hpp"

#include "RRL/io/ImageIO.hpp"

#include "RRL/rhi/RHIAPI.hpp"



#include "RRL/DebugMacros.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include <FLogging/FLogging.hpp>

#include <opencv2/imgproc.hpp>
#include <unordered_map>

namespace rrl::data {


/**
 * @brief Holds the running asset cached context.
 */
struct AssetCache {
    std::unordered_map<std::string, entt::entity> textures;
    std::unordered_map<std::string, entt::entity> meshes;
    AssetGCPolicy gc_policy { AssetGCPolicy::CASCADE_DELETE };
};


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

// --- Asset GC Helpers --------------------------------------------
static void IncrementAssetRef(entt::registry& registry, entt::entity asset) {
    if (asset != entt::null && registry.valid(asset)) {
        registry.get_or_emplace<AssetReferenceCounter>(asset).live_instances++;
    }
}
static void DecrementAssetRef(entt::registry& registry, entt::entity asset) {
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

// --- Asset GC Destructors ----------------------------------------
static void OnTextureLinkageDestroyed(entt::registry& registry, entt::entity entity) {
    auto& linkage = registry.get<TextureLinkage>(entity);
    DecrementAssetRef(registry, linkage.texture_asset);
}
static void OnMeshLinkageDestroyed(entt::registry& registry, entt::entity entity) {
    auto& linkage = registry.get<MeshLinkage>(entity);
    DecrementAssetRef(registry, linkage.mesh_asset);
}
static void OnMaterialDataDestroyed(entt::registry& registry, entt::entity entity) {
    auto& mat = registry.get<MaterialData>(entity);
    // Cascade dereference counting down to the textures
    DecrementAssetRef(registry, mat.albedo_map);
    DecrementAssetRef(registry, mat.normal_map);
    DecrementAssetRef(registry, mat.metallic_roughness_map);
}
static void OnMeshSourceDestroyed(entt::registry& registry, entt::entity entity) {
    auto& source = registry.get<MeshSourceComponent>(entity);
    if (source.mesh) {
        // Cascade dereference counting down to the textures
        for (const auto& mat_link : source.mesh->materials) {
            DecrementAssetRef(registry, mat_link.material_entity);
        }
    }
}



// --- Lifecycle ---------------------------------------------------
void InitializeAssetManager(entt::registry& registry) {
    registry.ctx().emplace<AssetCache>();
    
    // Hardware Resource Cleanups
    registry.on_destroy<TextureRuntimeComponent>().connect<&OnTextureRuntimeDestroyed>();
    registry.on_destroy<MeshRuntimeComponent>().connect<&OnMeshRuntimeDestroyed>();

    // Asset Garbage Collection
    registry.on_destroy<TextureLinkage>().connect<&OnTextureLinkageDestroyed>();
    registry.on_destroy<MeshLinkage>().connect<&OnMeshLinkageDestroyed>();
    registry.on_destroy<MaterialData>().connect<&OnMaterialDataDestroyed>();
    registry.on_destroy<MeshSourceComponent>().connect<&OnMeshSourceDestroyed>();
}
void SetAssetGCPolicy(entt::registry& registry, AssetGCPolicy policy) {
    if (registry.ctx().contains<AssetCache>()) {
        registry.ctx().get<AssetCache>().gc_policy = policy;
    }
}
void FreeUnusedAssets(entt::registry& registry) {
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
    }

    // Destroying the entity triggers the GC callbacks
    registry.destroy(asset_entity); 
}
void DestroyAllAssets(entt::registry& registry) {
    // Clear caches first
    if (registry.ctx().contains<AssetCache>()) {
        registry.ctx().get<AssetCache>().textures.clear();
        registry.ctx().get<AssetCache>().meshes.clear();
    }

    auto textures = registry.view<TextureSourceComponent>();
    registry.destroy(textures.begin(), textures.end());

    auto meshes = registry.view<MeshSourceComponent>();
    registry.destroy(meshes.begin(), meshes.end());

    auto materials = registry.view<MaterialData>();
    registry.destroy(materials.begin(), materials.end());
}



// --- Textures ----------------------------------------------------
entt::entity LoadTextureFromFile(entt::registry& registry, const std::string& filepath) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    auto& cache = registry.ctx().get<AssetCache>();

    if (cache.textures.find(filepath) != cache.textures.end()) {
        return cache.textures[filepath];
    }

    ImageData raw_image = rrl::io::LoadImage(filepath);
    if (raw_image.GetDataSize() == 0) {
        LOG_ERROR("AssetManager: Failed to load texture '{}'", filepath);
        return entt::null;
    }

    // Use move semantics to transfer ownership of the loaded data
    entt::entity tex_entity = CreateTexture(registry, std::move(raw_image));
    cache.textures[filepath] = tex_entity;

    return tex_entity;
}
entt::entity CreateTexture(entt::registry& registry, ImageData&& image_data) {
    entt::entity tex_entity = registry.create();
    
    auto& source = registry.emplace<TextureSourceComponent>(tex_entity);
    source.image = std::make_shared<ImageData>(std::move(image_data)); // Move the heavy vectors!
    source.version = 1;

    return tex_entity;
}
entt::entity CreateTexture(entt::registry& registry, uint32_t width, uint32_t height, 
                           ImageChannelLayout channels, ImageDataType type) {
    ImageData img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data_type = type;
    
    size_t byte_size = width * height * static_cast<size_t>(channels);
    img.data.resize(byte_size, 0);

    return CreateTexture(registry, std::move(img));
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
entt::entity LoadMeshFromFile(entt::registry& registry, const std::string& filepath) {
    RRL_ASSERT(registry.ctx().contains<AssetCache>(), "AssetManager not initialized!");
    auto& cache = registry.ctx().get<AssetCache>();

    if (cache.meshes.find(filepath) != cache.meshes.end()) {
        return cache.meshes[filepath];
    }

    // TODO: implement MeshIO
    /*
    MeshData raw_mesh = rrl::io::LoadMesh(filepath);
    if (raw_mesh.positions.empty()) {
        LOG_ERROR("AssetManager: Failed to load mesh '{}'", filepath);
        return entt::null;
    }
    entt::entity mesh_entity = CreateMesh(registry, std::move(raw_mesh));
    */
    
    // Placeholder
    entt::entity mesh_entity = CreateMesh(registry, MeshTopology::TRIANGLES);
    cache.meshes[filepath] = mesh_entity;

    return mesh_entity;
}
entt::entity CreateMesh(entt::registry& registry, MeshData&& mesh_data) {
    entt::entity mesh_entity = registry.create();
    
    auto& source = registry.emplace<MeshSourceComponent>(mesh_entity);
    source.mesh = std::make_shared<MeshData>(std::move(mesh_data)); // Move the geometry vectors
    source.version = 1;

    return mesh_entity;
}
entt::entity CreateMesh(entt::registry& registry, MeshTopology topology) {
    MeshData mesh;
    mesh.topology = topology;
    return CreateMesh(registry, std::move(mesh));
}
void BindMesh(entt::registry& registry, entt::entity world_object, entt::entity mesh_asset) {
    RRL_ASSERT(registry.valid(world_object), "BindMesh: Invalid world object entity!");
    if (mesh_asset != entt::null) {
        RRL_ASSERT(registry.valid(mesh_asset), "BindMesh: Invalid mesh asset entity!");
        RRL_ASSERT(registry.all_of<MeshSourceComponent>(mesh_asset), "BindMesh: Asset lacks a MeshSourceComponent!");
    }
    
    // Safely swap reference counts if overwriting an existing linkage
    if (auto* old_link = registry.try_get<MeshLinkage>(world_object)) {
        if (old_link->mesh_asset != mesh_asset) {
            DecrementAssetRef(registry, old_link->mesh_asset);
            IncrementAssetRef(registry, mesh_asset);
        }
    } else {
        IncrementAssetRef(registry, mesh_asset);
    }
    
    registry.emplace_or_replace<MeshLinkage>(world_object, mesh_asset);
}



// --- Materials ---------------------------------------------------
entt::entity CreateMaterial(entt::registry& registry, const MaterialData& mat_data) {
    entt::entity mat_entity = registry.create();
    registry.emplace<MaterialData>(mat_entity, mat_data);
    return mat_entity;
}
void BindMaterial(entt::registry& registry, entt::entity mesh_asset, entt::entity material_asset, 
                  uint32_t index_offset, uint32_t index_count) {
    RRL_ASSERT(registry.valid(mesh_asset), "BindMaterial: Invalid mesh asset!");
    RRL_ASSERT(registry.all_of<MeshSourceComponent>(mesh_asset), "BindMaterial: Lacks MeshSourceComponent!");
    RRL_ASSERT(registry.valid(material_asset), "BindMaterial: Invalid material asset!");
    RRL_ASSERT(registry.all_of<MaterialData>(material_asset), "BindMaterial: Lacks MaterialData!");

    auto& source = registry.get<MeshSourceComponent>(mesh_asset);
    if (!source.mesh) return;

    // Track the new dependency
    IncrementAssetRef(registry, material_asset);

    uint32_t count = (index_count == 0) ? static_cast<uint32_t>(source.mesh->indices.size()) : index_count;
    source.mesh->materials.push_back({index_offset, count, material_asset});
    source.version.fetch_add(1, std::memory_order_release);
}

void BindMaterialTexture(entt::registry& registry, entt::entity material_asset, entt::entity texture_asset, 
                 MaterialTextureType texture_type) {
    RRL_ASSERT(registry.valid(material_asset), "BindMaterialTexture: Invalid material asset!");
    if (texture_asset != entt::null) {
        RRL_ASSERT(registry.valid(texture_asset), "BindMaterialTexture: Invalid texture asset!");
        RRL_ASSERT(registry.all_of<TextureSourceComponent>(texture_asset), "BindMaterialTexture: Lacks TextureSourceComponent!");
    }

    if (registry.all_of<MaterialData>(material_asset)) {
        auto& mat = registry.get<MaterialData>(material_asset);
        entt::entity* target_slot = nullptr;
        
        switch (texture_type) {
            case MaterialTextureType::ALBEDO: target_slot = &mat.albedo_map; break;
            case MaterialTextureType::NORMAL_MAP: target_slot = &mat.normal_map; break;
            case MaterialTextureType::METALLIC_ROUGHNESS_MAP: target_slot = &mat.metallic_roughness_map; break;
        }

        if (target_slot && *target_slot != texture_asset) {
            // Safely swap the texture references
            DecrementAssetRef(registry, *target_slot);
            IncrementAssetRef(registry, texture_asset);
            *target_slot = texture_asset;
        }
    }
}



// --- UI Assets ---------------------------------------------------
void BindUITexture(entt::registry& registry, entt::entity ui_object, entt::entity texture_asset,
                   float screen_x, float screen_y, float screen_w, float screen_h) {
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
    
    registry.emplace_or_replace<TextureLinkage>(ui_object, texture_asset, screen_x, screen_y, screen_w, screen_h);
}

void UpdateUILayout(entt::registry& registry, entt::entity ui_object, 
                    float screen_x, float screen_y, float screen_w, float screen_h) {
    RRL_ASSERT(registry.valid(ui_object), "UpdateUILayout: Invalid UI object!");
    RRL_ASSERT(registry.all_of<TextureLinkage>(ui_object), "UpdateUILayout: Lacks TextureLinkage!");

    auto& layout = registry.get<TextureLinkage>(ui_object);
    layout.screen_x = screen_x;
    layout.screen_y = screen_y;
    layout.screen_w = screen_w;
    layout.screen_h = screen_h;
}



} // namespace rrl::data