// RRL/src/include/RRL/debug/RHIDebugger.hpp
#pragma once


#include <entt/entt.hpp>

#include "RRL/rhi/RHITypes.hpp"
#include "RRL/debug/RHIDebugTypes.hpp"


namespace rrl::debug::rhi {



// --- API ---------------------------------------------------------
/**
 * @brief Gets the current RHI debug report
 */
RHIDebugReport GetRHIDebugReport(entt::registry& registry);

/**
 * @brief Set a debug flag to the RHI
 */
void SetDebugFlag(entt::registry& registry, rrl::rhi::RHIDebugFlag flag, bool enable);



// --- Visual Debugging Utilities ----------------------------------
/**
 * @brief Spawns a physical wireframe frustum that matches a camera's parameters.
 */
entt::entity SpawnCameraFrustum(entt::registry& registry, entt::entity camera_entity, float visual_length = 2.0f);

/**
 * @brief Spawns a basic debug grid on the floor.
 */
entt::entity SpawnDebugGrid(entt::registry& registry, float size = 10.0f, int subdivisions = 10);


} // namespace rrl::debug::rhi