// src/include/RRL/scene/SceneEnvironment.hpp
#pragma once

#include "RRL/RRLTypes.hpp"
#include <glm/glm.hpp>

namespace rrl::scene {


/**
 * @brief Defines how the global scene background is rendered.
 */
enum class SceneEnvironmentType : uint8_t {
    SOLID_COLOR,            // Standard solid clear color (default)
    SKYBOX_CUBEMAP,         // 6-sided texture mapped to a box at infinity
    SKYBOX_EQUIRECTANGULAR, // 360-degree stitched 2D texture mapped to a sphere
    CUSTOM_MESH             // Advanced: Custom projection bowl/mesh for surround-view
};


/**
 * @brief Global context data holding the scene environment configuration.
 */
struct SceneEnvironment {
    SceneEnvironmentType type = SceneEnvironmentType::SOLID_COLOR;
    
    // The background color used when type == SOLID_COLOR, 
    // or as a fallback/border color for other types.
    glm::vec4 clear_color = {0.1f, 0.1f, 0.15f, 1.0f}; 
    
    // The texture asset (Cubemap or Equirectangular image)
    rrl::AssetID environment_texture = rrl::NULL_ASSET; 
    
    // Optional: Only used if type == CUSTOM_MESH
    rrl::AssetID custom_mesh = rrl::NULL_ASSET; 
};



} // namespace rrl::scene