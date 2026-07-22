// src/include/RRL/scene/SceneCache.hpp
#pragma once

#include "RRL/scene/SceneEnvironment.hpp"

#include <entt/entt.hpp>
#include <unordered_set>

namespace rrl::scene {


/**
 * @brief Internal cache stored in the EnTT context.
 * Holds global scene data such as the environment, and future 
 * parameters like global illumination settings.
 */
struct SceneCache {
    // Environment rendering data
    SceneEnvironment environment;

    std::unordered_set<entt::entity> active_objects;
};
    

} // namespace rrl::scene
