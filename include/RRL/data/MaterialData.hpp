// RRL/include/data/MaterialData.hpp
#pragma once


#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace rrl::data {
    
enum class MaterialTextureType : uint8_t {
    ALBEDO = 0,
    NORMAL_MAP = 1,
    METALLIC_ROUGHNESS_MAP = 2
};

/**
 * @brief Shading properties of a surface.
 * Acts as a component attached to a Material Entity in the ECS.
 */
struct MaterialData {
    glm::vec4 base_color { 1.0f, 1.0f, 1.0f, 1.0f };
    float roughness { 0.5f };
    float metallic { 0.0f };
    glm::vec3 emmission { 0.0f, 0.0f, 0.0f };
    
    // Texture links (points to enities holding TextureSource/Runtime components - Handled by the AssetManager)
    entt::entity albedo_map { entt::null };
    entt::entity normal_map { entt::null };
    entt::entity metallic_roughness_map { entt::null };
};


} // namespace rrl::data