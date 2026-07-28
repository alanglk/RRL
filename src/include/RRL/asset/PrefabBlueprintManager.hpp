// RRL/src/include/RRL/scene/PrefabBlueprintManager.hpp
#pragma once

#include "RRL/asset/AssetTypes.hpp"
#include "RRL/io/PrefabIO.hpp"

#include <entt/entt.hpp>



namespace rrl::asset {


/*
 * A prefab can be seen as a object hierarchy of sub-meshes. 
 * A 'car' can have a 'chasis', multiple 'wheel' objects and a 'interior' composition.
 * Then this car can live in a 'city'. The idea is that we can either spawn a whole 'city' 
 * with its internal relations, just a 'car' with its hierarchy or we can only spwan a 'wheel'.
*/

/*
 * These functions manages the prefab blueprints in memory liferime (loading and unloading).
 * See src/include/RRL/scene/PrefabInstanceManager.hpp for runtime prefabs instancing and destruction.
 */

// --- Lifecycle ---------------------------------------------------
/**
 * @brief Initializes the Prefab Manager, establishing the internal blueprint cache.
 */
void InitializePrefabManager(entt::registry& registry);

/**
 * @brief Takes ownership of raw prefab data, uploads assets to VRAM, 
 * and caches the lightweight blueprint.
 */
void PreloadPrefabBlueprint(entt::registry& registry, const rrl::asset::PrefabID& blueprint_id, rrl::io::IOPrefab&& prefab_data);

// TODO:
// void UnloadPrefabBlueprint(entt::registry& registry, const rrl::asset::PrefabID& blueprint_id) -> Will be implemented on the future

} // namespace rrl::asset
