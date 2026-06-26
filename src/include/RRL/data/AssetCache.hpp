// RRL/src/include/data/AssetCache.hpp

#include "RRL/data/AssetManager.hpp"
#include <unordered_map>

namespace rrl::data {


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