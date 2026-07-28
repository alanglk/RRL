// RRL/src/include/RRL/scene/PrefabManager.hpp
#pragma once

#include "RRL/asset/AssetTypes.hpp"
#include <entt/entt.hpp>


namespace rrl::scene {


/*
 * A prefab can be seen as a object hierarchy of sub-meshes. 
 * A 'car' can have a 'chasis', multiple 'wheel' objects and a 'interior' composition.
 * Then this car can live in a 'city'. The idea is that we can either spawn a whole 'city' 
 * with its internal relations, just a 'car' with its hierarchy or we can only spwan a 'wheel'.
*/

/*
 * These functions manages the prefab blueprints instancing and runtime destruction. 
 * See src/include/RRL/asset/PrefabBlueprintManager.hpp for in memory loading and unloading. 
 */

// --- Runtime -----------------------------------------------------
/**
 * @brief Instantiates a prefab into the world using a cached blueprint_id.
 * @brief Instantiates a prefab or any nested sub-prefab using dot-notation.
 * Examples: 
 * SpawnPrefab(registry, "rungholt_city")
 * SpawnPrefab(registry, "rungholt_city.vehicle_1")
 * SpawnPrefab(registry, "rungholt_city.vehicle_1.wheel_fl")
 * @return entt::entity The Root Entity, or entt::null if the blueprint isn't cached.
 */
entt::entity SpawnPrefabInstance(entt::registry& registry, const rrl::asset::PrefabID& blueprint_id);

/**
 * @brief Destroys a spawned prefab.
 */
void DestroyPrefabInstance(entt::registry& registry, entt::entity prefab_entity, bool force_asset_deletion = false);





} // namespace rrl::scene
