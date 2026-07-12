// RRL/src/include/data/AssetCache.hpp
#pragma once

#include <entt/entt.hpp>

#include "RRL/asset/AssetTypes.hpp"
#include <unordered_map>

namespace rrl::asset {


/**
 * @brief Holds the running asset cached context.
 */
struct AssetCache {
    std::unordered_map<TextureID, entt::entity> textures;
    std::unordered_map<MeshID, entt::entity> meshes;
    std::unordered_map<MaterialID, entt::entity> materials;
    AssetGCPolicy gc_policy { AssetGCPolicy::CASCADE_DELETE };
};


}