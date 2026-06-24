// RRL/include/scene/SceneManager.hpp
#pragma once

#include "RRL/io/PrefabIO.hpp"

#include <entt/entt.hpp>
#include <string>



namespace rrl::scene {


/*
 * A prefab can be seen as a object hierarchy of sub-meshes. 
 * A 'car' can have a 'chasis', multiple 'wheel' objects and a 'interior' composition.
 * Then this car can live in a 'city'. The idea is that we can either spawn a whole 'city' 
 * with its internal relations, just a 'car' with its hierarchy or we can only spwan a 'wheel'.
*/

// Prefab identifier
using BlueprintID = std::string;

/**
 * @brief Tag attached to every prefab spawned instance. 
* Stores the full path used to spawn it (e.g., "city" or "city.car.wheel").
 */
struct PrefabInstanceComponent { 
    BlueprintID path;
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
 * @brief Instantiates a prefab or any nested sub-prefab using dot-notation.
 * Examples: 
 * SpawnPrefab(registry, "rungholt_city")
 * SpawnPrefab(registry, "rungholt_city.vehicle_1")
 * SpawnPrefab(registry, "rungholt_city.vehicle_1.wheel_fl")
 * @return entt::entity The Root Entity, or entt::null if the blueprint isn't cached.
 */
entt::entity SpawnPrefab(entt::registry& registry, const BlueprintID& blueprint_id);

/**
 * @brief Destroys a spawned prefab.
 */
void DestroyPrefab(entt::registry& registry, entt::entity prefab_entity, bool force_asset_deletion = false);





} // namespace rrl::scene
