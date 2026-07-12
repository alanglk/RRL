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



} // namespace rrl::asset
