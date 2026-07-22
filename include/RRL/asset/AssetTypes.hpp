// RRL/include/data/AssetManager.hpp
#pragma once

#include <cstdint>
#include <string>


namespace rrl::asset {


/**
 * @brief Defines how the AssetManager handles assets whose reference count drops to 0.
 */
enum class AssetGCPolicy : uint8_t {
    CASCADE_DELETE = 0,     // Destroy the asset and cascade the deletion
    KEEP_CACHED_ASSETS = 1  // Keep the asset in memory for future usage
};


// Asset cache identifiers
using TextureID     = std::string;
using MeshID        = std::string;
using MaterialID    = std::string;


/*
 * A prefab can be seen as a object hierarchy of sub-meshes. 
 * A 'car' can have a 'chasis', multiple 'wheel' objects and a 'interior' composition.
 * Then this car can live in a 'city'. The idea is that we can either spawn a whole 'city' 
 * with its internal relations, just a 'car' with its hierarchy or we can only spwan a 'wheel'.
*/

// Prefab identifier
using PrefabID = std::string;



} // namespace rrl::asset
