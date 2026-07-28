// RRL/include/debug/AssetDebugTypes.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/asset/AssetTypes.hpp"
#include "RRL/RRLTypes.hpp"
#include <unordered_map>
#include <vector>


namespace rrl::debug {


// --- Debug Structs -----------------------------------------------
// Mesh -> Material -> Texture
struct RRL_API TextureDebugStats {
    rrl::asset::TextureID texture_id;
    AssetID asset_id;

    // How many times it is linked to other objects
    uint32_t ref_count;
};
struct RRL_API MeshDebugStats {
    rrl::asset::MeshID mesh_id;
    AssetID asset_id;

    // How many times it is linked to other objects
    uint32_t ref_count;

    std::vector<rrl::asset::MaterialID> material_links;
};
struct MaterialDebugStats {
    rrl::asset::MaterialID material_id;
    AssetID asset_id;

    // How many times it is linked to other objects
    uint32_t ref_count;
    
    std::vector<rrl::asset::TextureID> texture_links;
};


/**
 * @brief Assets debugging report
 */
template <typename T_Stats, typename T_ID>
struct RRL_API AssetDebugReport {
    // Assets that correctly exist in BOTH the registry and the cache
    std::unordered_map<T_ID, T_Stats> tracked_assets;
    
    // Leaked entities (they exist in RAM but are missing at cache)
    std::vector<AssetID> leaked_assets; 
    
    // Assets that exist in cache but do not exist in RAM
    std::vector<T_ID> dead_assets;
};


} // namespace rrl::debug
