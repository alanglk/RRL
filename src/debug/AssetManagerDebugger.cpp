// RRL/src/debug/AssetManagerDebugger.hpp

#include "RRL/debug/AssetManagerDebugger.hpp"
#include "RRL/data/AssetReferenceCounter.hpp"
#include "RRL/data/AssetCache.hpp"

#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MaterialData.hpp"

#include "RRL/DebugMacros.hpp"


namespace rrl::debug::data {

// --- Helpers -----------------------------------------------------
// Helper to grab reference counts safely
static uint32_t GetRefCount(entt::registry& registry, entt::entity entity) {
    if (registry.valid(entity)) {
        if (auto* ref = registry.try_get<rrl::data::AssetReferenceCounter>(entity)) {
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
AssetDebugReport<TextureDebugStats, rrl::data::TextureID> GetTextureDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::data::AssetCache>(), "AssetManager not initialized!");
    AssetDebugReport<TextureDebugStats, rrl::data::TextureID> report;
    if (!registry.ctx().contains<rrl::data::AssetCache>()) return report;

    // Retrieve cached objs
    const auto& cache = registry.ctx().get<rrl::data::AssetCache>().textures;
    auto reverse_cache = CreateReverseMap(cache);

    // Dead links (in cache but not in RAM)
    for (const auto& [id, entity] : cache) {
        if (!registry.valid(entity) || !registry.all_of<rrl::data::TextureSourceComponent>(entity)) {
            report.dead_assets.push_back(id);
        }
    }

    // Tracked and leaked assets
    auto view = registry.view<rrl::data::TextureSourceComponent>();
    for (auto entity : view) {
        auto it = reverse_cache.find(entity);
        if (it != reverse_cache.end()) {
            TextureDebugStats stats;
            stats.texture_id = it->second;
            stats.entity_id = entity;
            stats.ref_count = GetRefCount(registry, entity);
            report.tracked_assets[it->second] = stats;
        } else {
            report.leaked_assets.push_back(entity);
        }
    }

    return report;
}
AssetDebugReport<MeshDebugStats, rrl::data::MeshID> GetMeshDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::data::AssetCache>(), "AssetManager not initialized!");
    AssetDebugReport<MeshDebugStats, rrl::data::MeshID> report;
    if (!registry.ctx().contains<rrl::data::AssetCache>()) return report;

    // Retrieve cached objs
    const auto& mesh_cache = registry.ctx().get<rrl::data::AssetCache>().meshes;
    auto reverse_mesh_cache = CreateReverseMap(mesh_cache);
    const auto& mat_cache = registry.ctx().get<rrl::data::AssetCache>().materials;
    auto reverse_mat_cache = CreateReverseMap(mat_cache);

    // Dead links (in cache but not in RAM)
    for (const auto& [id, entity] : mesh_cache) {
        if (!registry.valid(entity) || !registry.all_of<rrl::data::MeshSourceComponent>(entity)) {
            report.dead_assets.push_back(id);
        }
    }

    // Tracked and leaked assets
    auto view = registry.view<rrl::data::MeshSourceComponent>();
    for (auto entity : view) {
        auto it = reverse_mesh_cache.find(entity);
        if (it != reverse_mesh_cache.end()) {
            MeshDebugStats stats;
            stats.mesh_id = it->second;
            stats.entity_id = entity;
            stats.ref_count = GetRefCount(registry, entity);

            // Resolve Material Links
            const auto& source = view.get<rrl::data::MeshSourceComponent>(entity);
            if (source.mesh) {
                for (const auto& mat_link : source.mesh->materials) {
                    auto mat_it = reverse_mat_cache.find(mat_link.material_entity);
                    if (mat_it != reverse_mat_cache.end()) {

                        // Avoid adding duplicates if multiple submeshes use the same material
                        if (std::find(stats.material_links.begin(), stats.material_links.end(), mat_it->second) == stats.material_links.end()) {
                            stats.material_links.push_back(mat_it->second);
                        }
                    } else {
                        stats.material_links.push_back("UNKNOWN_LEAKED_MATERIAL");
                    }
                }
            }
            report.tracked_assets[it->second] = stats;
        } else {
            report.leaked_assets.push_back(entity);
        }
    }

    return report;
}
AssetDebugReport<MaterialDebugStats, rrl::data::MaterialID> GetMaterialDebugReport(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<rrl::data::AssetCache>(), "AssetManager not initialized!");
    AssetDebugReport<MaterialDebugStats, rrl::data::MaterialID> report;
    if (!registry.ctx().contains<rrl::data::AssetCache>()) return report;
    
    // Retrieve cached objs
    const auto& mat_cache = registry.ctx().get<rrl::data::AssetCache>().materials;
    auto reverse_mat_cache = CreateReverseMap(mat_cache);
    const auto& tex_cache = registry.ctx().get<rrl::data::AssetCache>().textures;
    auto reverse_tex_cache = CreateReverseMap(tex_cache);

    // Dead links (in cache but not in RAM)
    for (const auto& [id, entity] : mat_cache) {
        if (!registry.valid(entity) || !registry.all_of<rrl::data::MaterialData>(entity)) {
            report.dead_assets.push_back(id);
        }
    }

    // Tracked and leaked assets
    auto view = registry.view<rrl::data::MaterialData>();
    for (auto entity : view) {
        auto it = reverse_mat_cache.find(entity);
        if (it != reverse_mat_cache.end()) {
            MaterialDebugStats stats;
            stats.material_id = it->second;
            stats.entity_id = entity;
            stats.ref_count = GetRefCount(registry, entity);

            // Resolve Texture Links
            const auto& data = view.get<rrl::data::MaterialData>(entity);
            std::vector<entt::entity> tex_entities = { data.albedo_map, data.normal_map, data.metallic_roughness_map, data.emissive_map };
            
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
            report.leaked_assets.push_back(entity);
        }
    }

    return report;
}

} // namespace rrl::debug::data