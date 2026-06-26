// RRL/include/debug/AssetManagerDebugger.hpp
#pragma once

#include "RRL/data/AssetManager.hpp"
#include <vector>

namespace rrl::debug::data {


// --- Debug Structs -----------------------------------------------
// Mesh -> Material -> Texture
struct TextureDebugStats {
    rrl::data::TextureID texture_id;
    entt::entity entity_id;

    // How many times it is linked to other objects
    uint32_t ref_count;
};
struct MeshDebugStats {
    rrl::data::MeshID mesh_id;
    entt::entity entity_id;

    // How many times it is linked to other objects
    uint32_t ref_count;

    std::vector<rrl::data::MaterialID> material_links;
};
struct MaterialDebugStats {
    rrl::data::MaterialID material_id;
    entt::entity entity_id;

    // How many times it is linked to other objects
    uint32_t ref_count;
    
    std::vector<rrl::data::TextureID> texture_links;
};


/**
 * @brief Assets debugging report
 */
template <typename T_Stats, typename T_ID>
struct AssetDebugReport {
    // Assets that correctly exist in BOTH the registry and the cache
    std::unordered_map<T_ID, T_Stats> tracked_assets;
    
    // Leaked entities (they exist in RAM but are missing at cache)
    std::vector<entt::entity> leaked_assets; 
    
    // Assets that exist in cache but do not exist in RAM
    std::vector<T_ID> dead_assets;
};


// --- API ---------------------------------------------------------
/**
 * @brief Cross-references the registry against the cache to generate a full texture diagnostic report.
 */
AssetDebugReport<TextureDebugStats, rrl::data::TextureID> GetTextureDebugReport(entt::registry& registry);
/**
 * @brief Cross-references the registry against the cache to generate a full mesh diagnostic report.
 */
AssetDebugReport<MeshDebugStats, rrl::data::MeshID> GetMeshDebugReport(entt::registry& registry);
/**
 * @brief Cross-references the registry against the cache to generate a full material diagnostic report.
 */
AssetDebugReport<MaterialDebugStats, rrl::data::MaterialID> GetMaterialDebugReport(entt::registry& registry);



} // namespace rrl::debug::data