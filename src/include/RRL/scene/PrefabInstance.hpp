// src/include/RRL/scene/PrefabInstance.hpp
#pragma once

#include "RRL/asset/AssetTypes.hpp"

namespace rrl::scene {

/**
 * @brief Tag attached to every prefab spawned instance. 
* Stores the full path used to spawn it (e.g., "city" or "city.car.wheel").
 */
struct PrefabInstanceComponent { 
    rrl::asset::PrefabID path;
};

} // namespace rrl::scene