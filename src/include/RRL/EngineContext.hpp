// RRL/src/include/RRL/EngineContext.hpp
#pragma once

#include <entt/entt.hpp>

namespace rrl {

/**
 * @brief Runtime context of the RRL library.
 */
struct EngineContext {
    entt::registry registry;
};


} // namespace rrl