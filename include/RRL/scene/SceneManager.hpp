// RRL/include/scene/SceneManager.hpp
#pragma once

#include "RRL/io/PrefabIO.hpp"

#include <entt/entt.hpp>
#include <string>



namespace rrl::scene {


// Prefab identifier
using BlueprintID = std::string;

/**
 * @brief Tag attached to every prefab spawned instance. 
 */
struct PrefabInstanceComponent { 
    BlueprintID blueprint_id;
};



// --- Lifecycle ---------------------------------------------------
/**
 * @brief Initializes the Scene Manager, establishing the internal blueprint cache.
 */
void InitializeSceneManager(entt::registry& registry);

/**
 * @brief Takes ownership of raw prefab data, uploads assets to VRAM, 
 * and caches the lightweight blueprint.
 */
void PreloadPrefabBlueprint(entt::registry& registry, const BlueprintID& blueprint_id, io::IOPrefab&& prefab_data);

/**
 * @brief Instantiates a prefab into the world using a cached blueprint_id.
 * @return entt::entity The Root Entity, or entt::null if the blueprint isn't cached.
 */
entt::entity SpawnPrefab(entt::registry& registry, const BlueprintID& blueprint_id);

/**
 * @brief Destroys a spawned prefab.
 */
void DestroyPrefab(entt::registry& registry, entt::entity prefab_entity, bool force_asset_deletion = false);





} // namespace rrl::scene
