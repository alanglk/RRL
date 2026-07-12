// RRL/src/include/RRL/debug/AssetDebugger.hpp
#pragma once

#include <entt/entt.hpp>
#include "RRL/debug/AssetDebugTypes.hpp"


namespace rrl::debug::asset {

// --- API ---------------------------------------------------------
/**
 * @brief Cross-references the registry against the cache to generate a full texture diagnostic report.
 */
AssetDebugReport<TextureDebugStats, rrl::asset::TextureID> GetTextureDebugReport(entt::registry& registry);
/**
 * @brief Cross-references the registry against the cache to generate a full mesh diagnostic report.
 */
AssetDebugReport<MeshDebugStats, rrl::asset::MeshID> GetMeshDebugReport(entt::registry& registry);
/**
 * @brief Cross-references the registry against the cache to generate a full material diagnostic report.
 */
AssetDebugReport<MaterialDebugStats, rrl::asset::MaterialID> GetMaterialDebugReport(entt::registry& registry);



} // namespace rrl::debug::data