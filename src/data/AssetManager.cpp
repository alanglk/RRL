// RRL/src/data/AssetManager.hpp

#include "RRL/data/AssetManager.hpp"
#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MeshData.hpp"

#include "RRL/io/ImageIO.hpp"

#include "RRL/rhi/RHIAPI.hpp"



#include "RRL/DebugMacros.hpp"
#include <FLogging/FLogging.hpp>

#include <unordered_map>

namespace rrl::data {

/**
 * @brief Holds the running asset cached context.
 */
struct AssetCache {
    std::unordered_map<std::string, entt::entity> textures;
    std::unordered_map<std::string, entt::entity> meshes;
};

/**
 * @brief EnTT Texture cleanup hook
 */
static void OnTextureRuntimeDestroyed(entt::registry& registry, entt::entity entity) {
    auto& runtime = registry.get<TextureRuntimeComponent>(entity);
    if (runtime.handle != rhi::TEXTURE_NULL) {
        rhi::DestroyTexture(registry, runtime.handle);
    }
}
/**
 * @brief EnTT Mesh cleanup hook
 */
/*
static void OnMeshRuntimeDestroyed(entt::registry& registry, entt::entity entity) {
    auto& runtime = registry.get<MeshRuntimeComponent>(entity);
    if (runtime.handle != rhi::MESH_NULL) {
        rhi::DestroyMesh(registry, runtime.handle);
    }
}
*/



// --- Lifecycle ---------------------------------------------------
void InitializeAssetManager(entt::registry& registry) {
    registry.ctx().emplace<AssetCache>();
    registry.on_destroy<TextureRuntimeComponent>().connect<&OnTextureRuntimeDestroyed>();
    // registry.on_destroy<MeshRuntimeComponent>().connect<&OnMeshRuntimeDestroyed>();
}
void DestroyAsset(entt::registry& registry, entt::entity asset_entity) {
    if (!registry.valid(asset_entity)) return;

    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        
        // Clean string caches if the entity was loaded from a file
        for (auto it = cache.textures.begin(); it != cache.textures.end(); ) {
            if (it->second == asset_entity) it = cache.textures.erase(it);
            else ++it;
        }
        for (auto it = cache.meshes.begin(); it != cache.meshes.end(); ) {
            if (it->second == asset_entity) it = cache.meshes.erase(it);
            else ++it;
        }
    }

    // Destroying the entity triggers the OnDestroy callback
    registry.destroy(asset_entity);
}
void DestroyAllAssets(entt::registry& registry) {
    // Destroy all entities holding asset components
    auto textures = registry.view<TextureSourceComponent>();
    registry.destroy(textures.begin(), textures.end());

    auto meshes = registry.view<MeshSourceComponent>();
    registry.destroy(meshes.begin(), meshes.end());

    auto materials = registry.view<MaterialData>();
    registry.destroy(materials.begin(), materials.end());

    // Clear the file cache maps
    if (registry.ctx().contains<AssetCache>()) {
        auto& cache = registry.ctx().get<AssetCache>();
        cache.textures.clear();
        cache.meshes.clear();
    }
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
        RRL_ASSERT(registry.all_of<MeshSourceComponent>(mesh_asset), "BindMesh: Asset entity provided lacks a MeshSourceComponent!");
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
    RRL_ASSERT(registry.valid(mesh_asset), "BindMaterial: Invalid mesh asset entity!");
    RRL_ASSERT(registry.all_of<MeshSourceComponent>(mesh_asset), "BindMaterial: Mesh entity lacks a MeshSourceComponent!");
    
    RRL_ASSERT(registry.valid(material_asset), "BindMaterial: Invalid material asset entity!");
    RRL_ASSERT(registry.all_of<MaterialData>(material_asset), "BindMaterial: Material entity lacks a MaterialData component!");

    if (registry.all_of<MeshSourceComponent>(mesh_asset)) {
        auto& source = registry.get<MeshSourceComponent>(mesh_asset);
        if (!source.mesh) return;

        uint32_t count = (index_count == 0) ? static_cast<uint32_t>(source.mesh->indices.size()) : index_count;
        source.mesh->materials.push_back({index_offset, count, material_asset});

        // Trigger an update to the GPU/RHI
        source.version.fetch_add(1, std::memory_order_release);
    }
}
void BindMaterialTexture(entt::registry& registry, entt::entity material_asset, entt::entity texture_asset, 
                 MaterialTextureType texture_type) {
    RRL_ASSERT(registry.valid(material_asset), "BindMaterialTexture: Invalid material asset entity!");
    if (texture_asset != entt::null) {
        RRL_ASSERT(registry.valid(texture_asset), "BindMaterialTexture: Invalid texture asset entity!");
        RRL_ASSERT(registry.all_of<TextureSourceComponent>(texture_asset), "BindMaterialTexture: Entity provided lacks a TextureSourceComponent!");
    }

    if (registry.all_of<MaterialData>(material_asset)) {
        auto& mat = registry.get<MaterialData>(material_asset);
        
        switch (texture_type) {
            case MaterialTextureType::ALBEDO: 
                mat.albedo_map = texture_asset; 
                break;
            case MaterialTextureType::NORMAL_MAP: 
                mat.normal_map = texture_asset; 
                break;
            case MaterialTextureType::METALLIC_ROUGHNESS_MAP: 
                mat.metallic_roughness_map = texture_asset; 
                break;
            default:
                break;
        }
    }
}



// --- UI Assets ---------------------------------------------------
void BindUITexture(entt::registry& registry, entt::entity ui_object, entt::entity texture_asset,
                   float screen_x, float screen_y, float screen_w, float screen_h) {
    RRL_ASSERT(registry.valid(ui_object), "BindUITexture: Invalid UI object entity!");
    
    if (texture_asset != entt::null) {
        RRL_ASSERT(registry.valid(texture_asset), "BindUITexture: Invalid texture asset entity!");
        RRL_ASSERT(registry.all_of<TextureSourceComponent>(texture_asset), 
                   "BindUITexture: The provided entity lacks a TextureSourceComponent!");
    }
    
    registry.emplace_or_replace<TextureLinkage>(ui_object, texture_asset, screen_x, screen_y, screen_w, screen_h);
}
void UpdateUILayout(entt::registry& registry, entt::entity ui_object, 
                    float screen_x, float screen_y, float screen_w, float screen_h) {
    RRL_ASSERT(registry.valid(ui_object), "UpdateUILayout: Invalid UI object entity!");
    RRL_ASSERT(registry.all_of<TextureLinkage>(ui_object), 
               "UpdateUILayout: Cannot adjust layout; entity is not linked to a UI texture!");

    auto& layout = registry.get<TextureLinkage>(ui_object);
    layout.screen_x = screen_x;
    layout.screen_y = screen_y;
    layout.screen_w = screen_w;
    layout.screen_h = screen_h;
}



} // namespace rrl::data