// RRL/include/Types.hpp
#pragma once

#include <cstdint>

namespace rrl {

/*
 * World objects and assets cannot be mixed. Assets can be seen as
 * special world hidden objects that have a garbage collector system
 * which continuously checks for unbined data. World objects reference
 * assets and assets can be updated no matter the linked objects.
 * Assets are:          Textures, Materials and Meshes.
 * World objects are:   Alive 3D object with possition; Alive 2D Ui frame...
*/
    

// World object id
enum class ObjectID : uint32_t {};
constexpr ObjectID NULL_OBJECT = static_cast<ObjectID>(0xFFFFFFFF);


// Asset id
enum class AssetID  : uint32_t {};
constexpr AssetID  NULL_ASSET  = static_cast<AssetID>(0xFFFFFFFF);


} // namespace rrl