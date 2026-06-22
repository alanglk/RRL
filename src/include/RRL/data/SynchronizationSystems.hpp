// RRL/src/include/data/TextureSystem.hpp
#pragma once

#include <entt/entt.hpp>


namespace rrl::data {

/**
 * @brief Synchronization loop to upload new texture data to the RHI backend.
 */
void SyncTexturesToRHI(entt::registry& registry);


} // namespace rrl::data