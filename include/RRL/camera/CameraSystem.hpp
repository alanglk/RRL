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
 * @brief Adds a camera component to an entity.
 * If the target_fbo is set to TARGET_NULL, the camera will be ignoned at the rendering phase.
 * If the target_fbo points to the same target as other camera, the other camera will point to TARGET_NULL.
 */
void AddCamera(entt::registry& registry, entt::entity entity, 
               const CameraModelVariant& model = PerspectiveModel{}, 
               rhi::RenderTargetHandle target_fbo = rhi::TARGET_MAIN);

/**
 * @brief Removes the camera from an entity.
 */
void RemoveCamera(entt::registry& registry, entt::entity entity);



// --- Setters -----------------------------------------------------
/**
 * @brief Updates the camera model and automatically.
 */
void SetCameraModel(entt::registry& registry, entt::entity entity, const CameraModelVariant& model);

/**
 * @brief Sets the rendering fbo target for the given entity camera. 
 * If the target_fbo points to the same target as other camera, the other camera will point to TARGET_NULL.
 */
void SetCameraTarget(entt::registry& registry, entt::entity entity, rhi::RenderTargetHandle target_fbo);

/**
 * @brief Sets the given entity as the primary camera, unsetting any other primary cameras.
 */
void SetPrimaryCamera(entt::registry& registry, entt::entity entity);




// --- Getters -----------------------------------------------------
/**
 * @brief Retrieves a COPY of the camera's current model.
 * To update the camera, modify this copy and pass it to SetCameraModel().
 */
CameraModelVariant GetCameraModel(entt::registry& registry, entt::entity entity);

/**
 * @brief Returns the entity ID of the current primary camera, or entt::null if none exists.
 */
entt::entity GetPrimaryCamera(entt::registry& registry);

/**
 * @brief Retrieves the cached View Matrix (LCS to CCS converted).
 */
const glm::mat4& GetViewMatrix(entt::registry& registry, entt::entity entity);

/**
 * @brief Retrieves the cached Projection Matrix (Target NDC converted).
 */
const glm::mat4& GetProjectionMatrix(entt::registry& registry, entt::entity entity);

/**
 * @brief Retrieves the cached View-Projection Matrix (Projection * View).
 */
const glm::mat4& GetViewProjectionMatrix(entt::registry& registry, entt::entity entity);



} // namespace rrl::camera