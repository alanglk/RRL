// RRL/src/debug/AssetManagerDebugger.hpp

#include "RRL/debug/AssetDebugger.hpp"
#include "RRL/asset/AssetReferenceCounter.hpp"
#include "RRL/asset/AssetCache.hpp"

#include "RRL/asset/TextureComponents.hpp"
#include "RRL/asset/MeshComponents.hpp"
#include "RRL/asset/MaterialComponents.hpp"

#include "RRL/EnttCasting.hpp"
#include "RRL/DebugMacros.hpp"


namespace rrl::debug::asset {

// --- Helpers -----------------------------------------------------
// Helper to grab reference counts safely
static uint32_t GetRefCount(entt::registry& registry, entt::entity entity) {
    if (registry.valid(entity)) {
        if (auto* ref = registry.try_get<rrl::asset::AssetReferenceCounter>(entity)) {
            return ref->live_instances;
        }
    }
    return 0;
}
// Invert a cache map
template<typename T_ID>
static std::unordered_map<entt::entity, T_ID> CreateReverseMap(const std::unordered_map<T_ID, entt::entity>& cache) {
    std::unordered_map<entt::entity, T_ID> reverse_map;
    for (const auto& [id, entity] : cache) {
        reverse_map[entity] = id;
    }
    return reverse_map;
}


// --- API ---------------------------------------------------------
AssetDebugReport<TextureDebugStats, rrl::asset::TextureID> GetTextureDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::asset::AssetCache>(), "AssetManager not initialized!");
    AssetDebugReport<TextureDebugStats, rrl::asset::TextureID> report;
    if (!registry.ctx().contains<rrl::asset::AssetCache>()) return report;

    // Retrieve cached objs
    const auto& cache = registry.ctx().get<rrl::asset::AssetCache>().textures;
    auto reverse_cache = CreateReverseMap(cache);

    // Dead links (in cache but not in RAM)
    for (const auto& [id, entity] : cache) {
        if (!registry.valid(entity) || !registry.all_of<rrl::asset::TextureSourceComponent>(entity)) {
            report.dead_assets.push_back(id);
        }
    }

    // Tracked and leaked assetsdata::
    auto view = registry.view<rrl::asset::TextureSourceComponent>();
    for (auto entity : view) {
        auto it = reverse_cache.find(entity);
        if (it != reverse_cache.end()) {
            TextureDebugStats stats;
            stats.texture_id    = it->second;
            stats.asset_id      = ToAssetID(entity);
            stats.ref_count     = GetRefCount(registry, entity);
            report.tracked_assets[it->second] = stats;
        } else {
            report.leaked_assets.push_back( ToAssetID(entity) );
        }
    }

    return report;
}
AssetDebugReport<MeshDebugStats, rrl::asset::MeshID> GetMeshDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::asset::AssetCache>(), "AssetManager not initialized!");
    AssetDebugReport<MeshDebugStats, rrl::asset::MeshID> report;
    if (!registry.ctx().contains<rrl::asset::AssetCache>()) return report;

    // Retrieve cached objs
    const auto& mesh_cache = registry.ctx().get<rrl::asset::AssetCache>().meshes;
    auto reverse_mesh_cache = CreateReverseMap(mesh_cache);
    const auto& mat_cache = registry.ctx().get<rrl::asset::AssetCache>().materials;
    auto reverse_mat_cache = CreateReverseMap(mat_cache);

    // Dead links (in cache but not in RAM)
    for (const auto& [id, entity] : mesh_cache) {
        if (!registry.valid(entity) || !registry.all_of<rrl::asset::MeshSourceComponent>(entity)) {
            report.dead_assets.push_back(id);
        }
    }

    // Tracked and leaked assets
    auto view = registry.view<rrl::asset::MeshSourceComponent>();
    for (auto entity : view) {
        auto it = reverse_mesh_cache.find(entity);
        if (it != reverse_mesh_cache.end()) {
            MeshDebugStats stats;
            stats.mesh_id   = it->second;
            stats.asset_id  = ToAssetID(entity);
            stats.ref_count = GetRefCount(registry, entity);
            report.tracked_assets[it->second] = stats;
        } else {
            report.leaked_assets.push_back( ToAssetID(entity) );
        }
    }

    return report;
}
AssetDebugReport<MaterialDebugStats, rrl::asset::MaterialID> GetMaterialDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::asset::AssetCache>(), "AssetManager not initialized!");
    AssetDebugReport<MaterialDebugStats, rrl::asset::MaterialID> report;
    if (!registry.ctx().contains<rrl::asset::AssetCache>()) return report;
    
    // Retrieve cached objs
    const auto& mat_cache = registry.ctx().get<rrl::asset::AssetCache>().materials;
    auto reverse_mat_cache = CreateReverseMap(mat_cache);
    const auto& tex_cache = registry.ctx().get<rrl::asset::AssetCache>().textures;
    auto reverse_tex_cache = CreateReverseMap(tex_cache);

    // Dead links (in cache but not in RAM)
    for (const auto& [id, entity] : mat_cache) {
        if (!registry.valid(entity) || !registry.all_of<rrl::asset::MaterialSourceComponent>(entity)) {
            report.dead_assets.push_back(id);
        }
    }

    // Tracked and leaked assets
    auto view = registry.view<rrl::asset::MaterialSourceComponent>();
    for (auto entity : view) {
        auto it = reverse_mat_cache.find(entity);
        if (it != reverse_mat_cache.end()) {
            MaterialDebugStats stats;
            stats.material_id   = it->second;
            stats.asset_id      = ToAssetID(entity);
            stats.ref_count     = GetRefCount(registry, entity);

            // Resolve Texture Links (Extracting inner data from the Source wrapper)
            const auto& source = view.get<rrl::asset::MaterialSourceComponent>(entity);
            const auto& data = source.data;

            std::vector<entt::entity> tex_entities = { 
                ToEntt(data.albedo_map), 
                ToEntt(data.normal_map), 
                ToEntt(data.metallic_roughness_map), 
                ToEntt(data.emissive_map) 
            };
            for (entt::entity tex_ent : tex_entities) {
                if (tex_ent != entt::null) {
                    auto tex_it = reverse_tex_cache.find(tex_ent);
                    if (tex_it != reverse_tex_cache.end()) {
                        stats.texture_links.push_back(tex_it->second);
                    } else {
                        stats.texture_links.push_back("UNKNOWN_LEAKED_TEXTURE");
                    }
                }
            }
            report.tracked_assets[it->second] = stats;
        } else {
            report.leaked_assets.push_back( ToAssetID(entity) );
        }
    }

    return report;
}

} // namespace rrl::debug::asset