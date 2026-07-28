// src/include/RRL/scene/SceneCache.hpp
#pragma once

#include "RRL/scene/SceneEnvironment.hpp"

#include <atomic>
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


    // Scene elements version tracking
    std::atomic<uint32_t> environment_version { 0 };
};
    
/**
 * @brief The RHI-side runtime cache. 
 * Managed entirely by the Render System. Do not touch from any other thread!
 */
struct SceneRuntimeCache {
    uint32_t cached_env_version { 0xFFFFFFFF }; // Current RHI scene environment version
};


} // namespace rrl::scene
