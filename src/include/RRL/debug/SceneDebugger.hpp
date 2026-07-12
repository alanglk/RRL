// RRL/src/include/RRL/debug/SceneDebugger.hpp
#pragma once

#include <entt/entt.hpp>
#include "RRL/debug/SceneDebugTypes.hpp"


namespace rrl::debug::scene {

// --- API ---------------------------------------------------------
/**
 * @brief Cross-references the registry against the PrefabCache to generate a full scene diagnostic report.
 */
SceneDebugReport GetSceneDebugReport(entt::registry& registry);


} // namespace rrl::debug::scene
