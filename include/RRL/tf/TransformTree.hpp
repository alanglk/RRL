// RRL/include/tf/TransformTree.hpp
#pragma once

#include <entt/entt.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rrl::tf {


/**
 * @brief Transformation node dependency management policies
 */
enum class TFDependencyPolicy : uint8_t {
    CASCADE_DELETE = 0,         // Remove the node if the parent gets destroyed
    REPARENT_TO_WORLD = 1       // Make a root node if its parent gets destroyed
};



// --- TF Runtime --------------------------------------------------
/**
 * @brief Register Tree Transform actions:
 *  - On TFRelationComponent destroy ->
 *          Update childrens in function of its TFDependencyPolicy 
 */
void RegisterTFActions(entt::registry& registry);

/**
 * @brief Ticks the Transform Tree. It iterates breadth-firstly starting at all 
 * root nodes and updates all the world transform matrices. It skips non changed branches.
 */
void UpdateTransformTree(entt::registry& registry);



// --- TF Adding / Updating / Getting ------------------------------

/**
 * @brief Adds a new transform to an entity as a world deatached TF
 */
void AddTransform(entt::registry& registry, entt::entity entity, 
                  const glm::vec3& position = {0.0f, 0.0f, 0.0f}, 
                  const glm::quat& rotation = {1.0f, 0.0f, 0.0f, 0.0f}, 
                  const glm::vec3& scale = {1.0f, 1.0f, 1.0f});
/**
 * @brief Adds a new transfrom to an entity and attachs it to another existing TF.
 * Keep in mind that the provided transform will be relative to the parent.
 * TF = PARENT -> CHILD
 */
void AddTransform(entt::registry& registry, entt::entity child_entity, entt::entity parent_entity,
                  const glm::vec3& position = {0.0f, 0.0f, 0.0f}, 
                  const glm::quat& rotation = {1.0f, 0.0f, 0.0f, 0.0f}, 
                  const glm::vec3& scale = {1.0f, 1.0f, 1.0f},
                  TFDependencyPolicy policy = TFDependencyPolicy::CASCADE_DELETE
                );

/**
 * @brief Sets / updates an already added transform position
 */
void SetLocalPosition(entt::registry& registry, entt::entity entity, const glm::vec3& pos);

/**
 * @brief Sets / updates an already added transform rotation
 */
void SetLocalRotation(entt::registry& registry, entt::entity entity, const glm::quat& rot);

/**
 * @brief Sets / updates an already added transform scale
 */
void SetLocalScale(entt::registry& registry, entt::entity entity, const glm::vec3& scale);

/**
 * @brief Gets the local transform position (ISO 8855 CS)
 */
glm::vec3 GetLocalPosition(entt::registry& registry, entt::entity entity);

/**
 * @brief Gets the local transform rotation (quaternion)
 */
glm::quat GetLocalRotation(entt::registry& registry, entt::entity entity);

/**
 * @brief Gets the local transform scale 
 */
glm::vec3 GetLocalScale(entt::registry& registry, entt::entity entity);

/**
 * @brief Retrieves the final, absolute 4x4 world matrix (ISO 8855 CS).
 */
const glm::mat4& GetWorldMatrix(entt::registry& registry, entt::entity entity);



// --- TF Hierarchy ------------------------------------------------
/**
 * @brief Attaches a child to a parent with a specific dependency policy.
 */
void AttachChild(entt::registry& registry, entt::entity parent, entt::entity child, 
                 TFDependencyPolicy policy = TFDependencyPolicy::CASCADE_DELETE);

/**
 * @brief Detaches a child from its parent, making it a root node in the World Coordinate System.
 */
void DetachChild(entt::registry& registry, entt::entity child);


}