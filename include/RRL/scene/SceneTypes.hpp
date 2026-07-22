// RRL/include/scene/SceneTypes.hpp
#pragma once


#include <cstdint>
#include <string>


namespace rrl::scene {

/*
 * A prefab can be seen as a object hierarchy of sub-meshes. 
 * A 'car' can have a 'chasis', multiple 'wheel' objects and a 'interior' composition.
 * Then this car can live in a 'city'. The idea is that we can either spawn a whole 'city' 
 * with its internal relations, just a 'car' with its hierarchy or we can only spwan a 'wheel'.
*/

// Prefab identifier
using PrefabID = std::string;


} // namespace rrl::scene
