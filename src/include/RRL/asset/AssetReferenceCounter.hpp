// RRL/src/include/RRL/asset/AssetReferenceCounter.hpp
#pragma once

#include <cstdint>

namespace rrl::asset {


/**
 * @brief Kept hidden inside the asset entity to track usage
 */
struct AssetReferenceCounter {
    uint32_t live_instances { 0 };
};


} // namespace rrl::asset
