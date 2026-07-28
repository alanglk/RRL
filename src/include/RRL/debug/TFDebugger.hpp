// RRL/include/debug/TFDebugger.cpp
#pragma once

#include <entt/entt.hpp>
#include "RRL/debug/TFDebugTypes.hpp"


namespace rrl::debug::tf {

// --- API ---------------------------------------------------------
/**
 * @brief Parses the ECS registry to reconstruct the Transform Tree and evaluate its health.
 */
TFDebugReport GetTransformTreeDebugReport(entt::registry& registry);


} // namespace rrl::debug::tf
