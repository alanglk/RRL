// RRL/src/include/RRL/data/AssetReferenceCounter.hpp
#pragma once

#include <cstdint>

namespace rrl::data {


/**
 * @brief Kept hidden inside the asset entity to track usage
 */
struct AssetReferenceCounter {
    uint32_t live_instances { 0 };
};


} // namespace rrl::data
