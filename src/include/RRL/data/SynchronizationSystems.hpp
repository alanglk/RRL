// RRL/src/include/data/TextureSystem.hpp
#pragma once

#include <entt/entt.hpp>


namespace rrl::data {

/**
 * @brief Synchronization loop to upload new texture data to the RHI backend.
 */
void SyncTexturesToRHI(entt::registry& registry);

/**
 * @brief Synchronization loop to upload new mesh geometry data to the RHI backend.
 */
void SyncMeshesToRHI(entt::registry& registry);

/**
 * @brief Synchronization loop to resolve material linked data for not resolved materials.
 */
void SyncMaterialsToRHI(entt::registry& registry);



} // namespace rrl::data