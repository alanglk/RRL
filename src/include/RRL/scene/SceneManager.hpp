// RRL/src/include/RRL/scene/SceneManager.hpp
#pragma once


#include <glm/glm.hpp>
#include <entt/entt.hpp>

#include "RRL/RRLTypes.hpp"


namespace rrl::scene {


// --- Lifecycle ---------------------------------------------------
/**
 * @brief Initializes the SceneCache in the registry context and 
 *      also calls the Prefab manager initialization function.
 */
void InitializeSceneManager(entt::registry& registry);
/**
 * @brief Spawns an empty object on the scene.
 */
entt::entity SpawnObject(entt::registry& registry);
/**
 * @brief Destroys an object from the scene.
 */
void DestroyObject(entt::registry& registry, entt::entity world_object);
/**
 * @brief Clears the scene.
 */
void DestroyAllObjects(entt::registry& registry);



// --- Environment -------------------------------------------------
/**
 * @brief Internal setters and getters for the Scene Environment.
 */
void SetEnvironmentColor(entt::registry& registry, const glm::vec4& color);
void SetEnvironmentCubemap(entt::registry& registry, rrl::AssetID cubemap_asset);
void SetEnvironmentEquirectangular(entt::registry& registry, rrl::AssetID texture_asset);
void SetEnvironmentCustomMesh(entt::registry& registry, rrl::AssetID texture_asset, rrl::AssetID mesh_asset);



} // namespace rrl::scene

