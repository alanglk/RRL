// RRL/include/modules/CameraModule.hpp
#pragma once

#include <RRL/rrl_export.h>
#include <cstdint>

#include "RRL/camera/CameraModels.hpp"
#include "RRL/camera/CameraConventions.hpp"

#include "RRL/rhi/RHITypes.hpp"
#include "RRL/RRLTypes.hpp"


namespace rrl {
    

//  RRL::Engine runtime context.
struct EngineContext;


class RRL_API CameraModule {
public:
    // --- Constructor / Destructor ------------------------------------
    explicit CameraModule(EngineContext* ctx);
    ~CameraModule();

    // Rule of five (this class is owned by RRLEngine)
    CameraModule(const CameraModule&)               = delete;
    CameraModule& operator=(const CameraModule&)    = delete;
    CameraModule(CameraModule&&)                    = delete;
    CameraModule& operator=(CameraModule&&)         = delete;


    // --- Camera Runtime ----------------------------------------------
    /**
     * @brief Computes View, Projection, and VP matrices for all cameras based on the target RHI.
     * Should be called once per frame, strictly AFTER tf::UpdateTransformTree.
     */
    void UpdateCameras(const rrl::camera::NDCConvention& ndc_target);



    // --- Lifecycle ---------------------------------------------------
    /**
     * @brief Spawns a new camera
     * If the target_fbo is set to TARGET_NULL, the camera will be ignoned at the rendering phase.
     * If the target_fbo points to the same target as other camera, the other camera will point to TARGET_NULL.
     */
    ObjectID SpawnCamera(
        const rrl::camera::CameraModelVariant& model = rrl::camera::PerspectiveModel{}, 
        rrl::rhi::ResourceID target_fbo = rhi::TARGET_MAIN,
        rrl::rhi::RHIRenderLayerMask layer = rhi::RHIRenderLayerMask::LAYER_ALL
    );
    /**
     * @brief Removes the camera from the registry. This completely destroys the camera entity.
     */
    void DestroyCamera(ObjectID cam_obj);
    /**
     * @brief Destroys all currently active cameras in the ECS.
     */
    void DestroyAllCameras();



    // --- Setters -----------------------------------------------------
    /**
     * @brief Updates the camera model of a spawned camera.
     */
    void SetCameraModel(ObjectID cam_obj, const rrl::camera::CameraModelVariant& model);
    /**
     * @brief Sets the rendering fbo target for the given entity camera. 
     * If the target_fbo points to the same target as other camera, the other camera will point to TARGET_NULL.
     */
    void SetCameraTarget(ObjectID cam_obj, rrl::rhi::ResourceID target_fbo);
    /**
     * @brief Sets the rendering culling mask of the camera
     */
    void SetCameraLayer(ObjectID cam_obj, rrl::rhi::RHIRenderLayerMask layer);
    /**
     * @brief Sets the rendering priority of the camera.
     * Low value    -> Higher priority
     * High value   -> Lower priority
     */
    void SetCameraRenderPriority(ObjectID cam_obj, uint32_t render_priority);
    /**
     * @brief Sets the given entity as the primary camera, unsetting any other primary cameras.
     */
    void SetPrimaryCamera(ObjectID cam_obj);
    /**
     * @brief Moves the camera to a specific position and immediately rotates it to look at a target.
     * Assumes ISO 8855 (+X Forward, +Y Left, +Z Up).
     */
    void SetCameraPositionAndLookAt(ObjectID cam_obj, const glm::vec3& pos, const glm::vec3& target, const glm::vec3& world_up = glm::vec3(0.0f, 0.0f, 1.0f));
    /**
     * @brief Rotates the camera to look at a target, leaving its current world position intact.
     * Assumes ISO 8855 (+X Forward, +Y Left, +Z Up).
     */
    void SetCameraLookAt(ObjectID cam_obj, const glm::vec3& target, const glm::vec3& world_up = glm::vec3(0.0f, 0.0f, 1.0f));



    // --- Getters -----------------------------------------------------
    /**
     * @brief Retrieves a list of all currently active camera entities in the registry.
     */
    std::vector<ObjectID> GetAllCameras();
    /**
     * @brief Retrieves a COPY of the camera's current model.
     * To update the camera, modify this copy and pass it to SetCameraModel().
     */
    rrl::camera::CameraModelVariant GetCameraModel(ObjectID cam_obj);
    /**
     * @brief Returns the entity ID of the current primary camera, or entt::null if none exists.
     */
    ObjectID GetPrimaryCamera();
    /**
     * @brief Retrieves the cached View Matrix (LCS to CCS converted).
     */
    const glm::mat4& GetViewMatrix(ObjectID cam_obj);
    /**
     * @brief Retrieves the cached Projection Matrix (Target NDC converted).
     */
    const glm::mat4& GetProjectionMatrix(ObjectID cam_obj);
    /**
     * @brief Retrieves the cached View-Projection Matrix (Projection * View).
     */
    const glm::mat4& GetViewProjectionMatrix(ObjectID cam_obj);



private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl