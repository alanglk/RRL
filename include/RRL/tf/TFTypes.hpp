// RRL/include/tf/TFTypes.hpp
#pragma once

#include <cstdint>


namespace rrl::tf {


/**
 * @brief Transformation node dependency management policies
 */
enum class TFDependencyPolicy : uint8_t {
    CASCADE_DELETE = 0,         // Remove the node if the parent gets destroyed
    REPARENT_TO_WORLD = 1       // Make a root node if its parent gets destroyed
};



} // namespace rrl::tf
