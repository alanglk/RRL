// RRL/include/data/MaterialData.hpp
#pragma once


#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace rrl::data {
    
enum class MaterialTextureType : uint8_t {
    ALBEDO = 0,
    NORMAL_MAP = 1,
    METALLIC_ROUGHNESS_MAP = 2,
    EMISSIVE_MAP = 3
};

enum class ShadingModel : uint8_t {
    // Material shaders (often used with loaded models)
    UNLIT = 0,                      // Ignores lights (UI, Emissive only, or basic LiDAR)
    PHONG = 1,                      // Phong model
    PBR_OPAQUE = 2,                 // Physically Based Rendering
    PBR_TRANSPARENT = 3,            // PBR with alpha blending
    
    // System shaders (used with specific data)
    POINT_CLOUD = 100,              // Specific shader for point clouds
    UI2D = 101                      // Specific shader for UI 2D textures
};

/**
 * @brief Shading properties of a surface.
 * Acts as a component attached to a Material Entity in the ECS.
 */
struct MaterialData {
    ShadingModel shading_model { ShadingModel::UNLIT };

    glm::vec4   base_color { 1.0f, 1.0f, 1.0f, 1.0f };
    float       roughness { 0.5f };
    float       metallic { 0.0f };
    glm::vec3   emmission { 0.0f, 0.0f, 0.0f };
    
    // Texture links (points to enities holding TextureSource/Runtime components - Handled by the AssetManager)
    entt::entity albedo_map { entt::null };             // MaterialTextureType::ALBEDO
    entt::entity normal_map { entt::null };             // MaterialTextureType::NORMAL_MAP
    entt::entity metallic_roughness_map { entt::null }; // MaterialTextureType::METALLIC_ROUGHNESS_MAP
    entt::entity emissive_map { entt::null };           // MaterialTextureType::EMISSIVE_MAP
};


} // namespace rrl::data