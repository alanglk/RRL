// RRL/src/include/RRL/camera/CameraSystem.hpp
#pragma once

#include "RRL/camera/CameraModels.hpp"
#include "RRL/camera/CameraConventions.hpp"
#include "RRL/rhi/RHIBackend.hpp"

#include <entt/entt.hpp>


namespace rrl::camera {
    
// --- Camera Runtime ----------------------------------------------
/**
 * @brief Computes View, Projection, and VP matrices for all cameras based on the target RHI.
 * Should be called once per frame, strictly AFTER tf::UpdateTransformTree.
 */
void UpdateCameras(entt::registry& registry, const NDCConvention& ndc_target);



// --- Lifecycle ---------------------------------------------------
/**
 * @brief Spawns a new camera
 * If the target_fbo is set to TARGET_NULL, the camera will be ignoned at the rendering phase.
 * If the target_fbo points to the same target as other camera, the other camera will point to TARGET_NULL.
 */
entt::entity SpawnCamera(entt::registry& registry, const CameraModelVariant& model = PerspectiveModel{}, rhi::RenderTargetHandle target_fbo = rhi::TARGET_MAIN);
/**
 * @brief Removes the camera from the registry. This completely destroys the camera entity.
 */
void DestroyCamera(entt::registry& registry, entt::entity cam_entity);
/**
 * @brief Destroys all currently active cameras in the ECS.
 */
void DestroyAllCameras(entt::registry& registry);



// --- Setters -----------------------------------------------------
/**
 * @brief Updates the camera model of a spawned camera.
 */
void SetCameraModel(entt::registry& registry, entt::entity cam_entity, const CameraModelVariant& model);
/**
 * @brief Sets the rendering fbo target for the given entity camera. 
 * If the target_fbo points to the same target as other camera, the other camera will point to TARGET_NULL.
 */
void SetCameraTarget(entt::registry& registry, entt::entity cam_entity, rhi::RenderTargetHandle target_fbo);
/**
 * @brief Sets the given entity as the primary camera, unsetting any other primary cameras.
 */
void SetPrimaryCamera(entt::registry& registry, entt::entity cam_entity);
/**
 * @brief Moves the camera to a specific position and immediately rotates it to look at a target.
 * Assumes ISO 8855 (+X Forward, +Y Left, +Z Up).
 */
void SetCameraPositionAndLookAt(entt::registry& registry, entt::entity cam_entity, const glm::vec3& pos, const glm::vec3& target, const glm::vec3& world_up = glm::vec3(0.0f, 0.0f, 1.0f));
/**
 * @brief Rotates the camera to look at a target, leaving its current world position intact.
 * Assumes ISO 8855 (+X Forward, +Y Left, +Z Up).
 */
void SetCameraLookAt(entt::registry& registry, entt::entity cam_entity, const glm::vec3& target, const glm::vec3& world_up = glm::vec3(0.0f, 0.0f, 1.0f));



// --- Getters -----------------------------------------------------
/**
 * @brief Retrieves a list of all currently active camera entities in the registry.
 */
std::vector<entt::entity> GetAllCameras(entt::registry& registry);
/**
 * @brief Retrieves a COPY of the camera's current model.
 * To update the camera, modify this copy and pass it to SetCameraModel().
 */
CameraModelVariant GetCameraModel(entt::registry& registry, entt::entity cam_entity);
/**
 * @brief Returns the entity ID of the current primary camera, or entt::null if none exists.
 */
entt::entity GetPrimaryCamera(entt::registry& registry);
/**
 * @brief Retrieves the cached View Matrix (LCS to CCS converted).
 */
const glm::mat4& GetViewMatrix(entt::registry& registry, entt::entity cam_entity);
/**
 * @brief Retrieves the cached Projection Matrix (Target NDC converted).
 */
const glm::mat4& GetProjectionMatrix(entt::registry& registry, entt::entity cam_entity);
/**
 * @brief Retrieves the cached View-Projection Matrix (Projection * View).
 */
const glm::mat4& GetViewProjectionMatrix(entt::registry& registry, entt::entity cam_entity);



} // namespace rrl::camera