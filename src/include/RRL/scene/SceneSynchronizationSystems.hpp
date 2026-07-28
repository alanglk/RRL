// src/include/RRL/scene/SceneSynchronizationSystems.hpp
#pragma once

#include <entt/entt.hpp>

namespace rrl::scene {

/**
 * @brief Synchronization loop to synchronize scene data (environment, lighting...) with the RHI backend.
 */
void SyncSceneToRHI(entt::registry& registry);


} // namespace rrl::scene 