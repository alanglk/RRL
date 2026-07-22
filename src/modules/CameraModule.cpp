// RRL/src/modules/CameraModule.cpp

#include "RRL/modules/CameraModule.hpp"
#include "RRL/camera/CameraManager.hpp"

#include "RRL/EngineContext.hpp"
#include "RRL/EnttCasting.hpp"

namespace rrl {


// --- Constructor / Destructor ------------------------------------
CameraModule::CameraModule(EngineContext* ctx): m_ctx(ctx) {
    rrl::camera::InitializeCameraManager(m_ctx->registry);
}
CameraModule::~CameraModule() {
    if (m_ctx != nullptr) {
        DestroyAllCameras();
    }
}

// --- Camera Runtime ----------------------------------------------
void CameraModule::UpdateCameras(const rrl::camera::NDCConvention& ndc_target) {
    rrl::camera::UpdateCameras(m_ctx->registry, ndc_target);
}

// --- Lifecycle ---------------------------------------------------
ObjectID CameraModule::SpawnCamera(
    const rrl::camera::CameraModelVariant& model, 
    rrl::rhi::RenderTargetHandle target_fbo,
    rrl::rhi::RHIRenderLayer layer
) {
    return ToObjectID( rrl::camera::SpawnCamera(m_ctx->registry, model, target_fbo, layer) );
}

void CameraModule::DestroyCamera(ObjectID cam_obj) {
    rrl::camera::DestroyCamera(m_ctx->registry, ToEntt(cam_obj) );
}

void CameraModule::DestroyAllCameras() {
    rrl::camera::DestroyAllCameras(m_ctx->registry);
}

// --- Setters -----------------------------------------------------
void CameraModule::SetCameraModel(ObjectID cam_obj, const rrl::camera::CameraModelVariant& model) {
    rrl::camera::SetCameraModel(m_ctx->registry, ToEntt(cam_obj), model);
}
void CameraModule::SetCameraTarget(ObjectID cam_obj, rhi::RenderTargetHandle target_fbo) {
    rrl::camera::SetCameraTarget(m_ctx->registry, ToEntt(cam_obj), target_fbo);
}
void CameraModule::SetCameraLayer(ObjectID cam_obj, rhi::RHIRenderLayer layer) {
    rrl::camera::SetCameraLayer(m_ctx->registry, ToEntt(cam_obj), layer);
}
void CameraModule::SetCameraRenderPriority(ObjectID cam_obj, uint32_t render_priority) {
    rrl::camera::SetCameraPriority(m_ctx->registry, ToEntt(cam_obj), render_priority);
}
void CameraModule::SetPrimaryCamera(ObjectID cam_obj) {
    rrl::camera::SetPrimaryCamera(m_ctx->registry, ToEntt(cam_obj));
}

void CameraModule::SetCameraPositionAndLookAt(ObjectID cam_obj, const glm::vec3& pos, const glm::vec3& target, const glm::vec3& world_up) {
    rrl::camera::SetCameraPositionAndLookAt(m_ctx->registry, ToEntt(cam_obj), pos, target, world_up);
}

void CameraModule::SetCameraLookAt(ObjectID cam_obj, const glm::vec3& target, const glm::vec3& world_up) {
    rrl::camera::SetCameraLookAt(m_ctx->registry, ToEntt(cam_obj), target, world_up);
}

// --- Getters -----------------------------------------------------
std::vector<ObjectID> CameraModule::GetAllCameras() {
    std::vector<entt::entity> internal_cams = rrl::camera::GetAllCameras(m_ctx->registry);
    
    // Convert entt::entity array to ObjectID (uint32_t) array
    std::vector<ObjectID> out_cams;
    out_cams.reserve(internal_cams.size());
    for(auto entity : internal_cams) {
        out_cams.push_back(ToObjectID(entity));
    }
    return out_cams;
}

rrl::camera::CameraModelVariant CameraModule::GetCameraModel(ObjectID cam_obj) {
    return rrl::camera::GetCameraModel(m_ctx->registry, ToEntt(cam_obj));
}

ObjectID CameraModule::GetPrimaryCamera() {
    return ToObjectID( rrl::camera::GetPrimaryCamera(m_ctx->registry) );
}

const glm::mat4& CameraModule::GetViewMatrix(ObjectID cam_obj) {
    return rrl::camera::GetViewMatrix(m_ctx->registry, ToEntt(cam_obj));
}

const glm::mat4& CameraModule::GetProjectionMatrix(ObjectID cam_obj) {
    return rrl::camera::GetProjectionMatrix(m_ctx->registry, ToEntt(cam_obj));
}

const glm::mat4& CameraModule::GetViewProjectionMatrix(ObjectID cam_obj) {
    return rrl::camera::GetViewProjectionMatrix(m_ctx->registry, ToEntt(cam_obj));
}





} // namespace rrl