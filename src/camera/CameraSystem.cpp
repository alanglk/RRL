// RRL/src/camera/CameraSystem.cpp

#include "RRL/camera/CameraSystem.hpp"
#include "RRL/camera/CameraComponents.hpp"
#include "RRL/rhi/RHIBackend.hpp"

#include "RRL/tf/TFComponents.hpp"
#include "RRL/tf/TransformTree.hpp"

#include <FLogging/FLogging.hpp>

#include "RRL/DebugMacros.hpp"


namespace rrl::camera {
    

// Matrix to map ISO 8855 (X-front, Y-left, Z-up) 
// to standard OpenGL View Space (X-right, Y-up, Z-backward)
constexpr glm::mat4 LCS_TO_CCS = glm::mat4(
    0.0f,  0.0f, -1.0f,  0.0f, 
   -1.0f,  0.0f,  0.0f,  0.0f, 
    0.0f,  1.0f,  0.0f,  0.0f, 
    0.0f,  0.0f,  0.0f,  1.0f
);



// --- Helpers -----------------------------------------------------
static glm::quat CalculateLookAtRotation(const glm::vec3& eye, const glm::vec3& target, const glm::vec3& world_up) {
    glm::vec3 forward = target - eye;
    if (glm::length(forward) < 0.0001f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    
    forward = glm::normalize(forward);
    glm::vec3 left = glm::normalize(glm::cross(world_up, forward));
    glm::vec3 true_up = glm::cross(forward, left);
    
    return glm::quat_cast(glm::mat3(forward, left, true_up));
}



// --- Camera Runtime ----------------------------------------------
void UpdateCameras(entt::registry& registry, const NDCConvention& ndc_target) {
    auto view = registry.view<CameraComponent, tf::TFWorldTransformComponent>();

    for (auto entity : view) {
        auto& cam = view.get<CameraComponent>(entity);
        const auto& world_tf = view.get<tf::TFWorldTransformComponent>(entity);


        bool spatial_changed = false;
        bool intrinsic_changed = cam.intrinsic_dirty;
        if (!registry.all_of<CameraRuntimeComponent>(entity)) {
            registry.emplace<CameraRuntimeComponent>(entity);
            spatial_changed = true;
            intrinsic_changed = true;
        }
        auto& runtime = registry.get<CameraRuntimeComponent>(entity);

        spatial_changed = spatial_changed || (runtime.cached_tf_version != world_tf.version);
        intrinsic_changed = intrinsic_changed || (runtime.cached_ndc_target != ndc_target);

        // Skip entity
        if (!spatial_changed && !intrinsic_changed) {
            continue; 
        }


        // Update the View Matrix
        if (spatial_changed) {
            runtime.view_matrix = LCS_TO_CCS * glm::inverse(world_tf.matrix);
            runtime.cached_tf_version = world_tf.version; // Sync the versions
        }


        // Update the Projection Matrix
        if (intrinsic_changed) {
            runtime.projection_matrix = std::visit([&ndc_target](auto&& model) -> glm::mat4 {
                using T = std::decay_t<decltype(model)>;
                glm::mat4 proj(1.0f);

                if constexpr (std::is_same_v<T, PerspectiveModel>) {
                    proj = glm::perspectiveRH_NO(model.fov_y_radians, model.aspect_ratio, model.z_near, model.z_far);
                    if (ndc_target.depth_range == NDCDepth::ZERO_TO_ONE) {
                        proj = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 0.5f)) * proj;
                        proj = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * proj;
                    }
                    if (ndc_target.y_direction == NDCYDirection::UP) { proj[1][1] *= -1.0f; }
                } 
                else if constexpr (std::is_same_v<T, OrthographicModel>) {
                    float half_w = model.width * 0.5f; float half_h = model.height * 0.5f;
                    proj = glm::orthoRH_NO(-half_w, half_w, -half_h, half_h, model.z_near, model.z_far);
                    if (ndc_target.depth_range == NDCDepth::ZERO_TO_ONE) {
                        proj[2][2] = -1.0f / (model.z_far - model.z_near);
                        proj[3][2] = -model.z_near / (model.z_far - model.z_near);
                    }
                    if (ndc_target.y_direction == NDCYDirection::UP) { proj[1][1] *= -1.0f; }
                }
                else if constexpr (std::is_same_v<T, PinholeModel>) {
                    float w = static_cast<float>(model.width_px); float h = static_cast<float>(model.height_px);
                    proj = glm::mat4(0.0f);
                    proj[0][0] = (2.0f * model.fx) / w;
                    proj[1][1] = (2.0f * model.fy) / h;
                    proj[2][0] = (2.0f * model.cx) / w - 1.0f;
                    proj[2][1] = (2.0f * model.cy) / h - 1.0f;
                    proj[2][3] = 1.0f;
                    
                    if (ndc_target.depth_range == NDCDepth::ZERO_TO_ONE) {
                        proj[2][2] = model.z_far / (model.z_far - model.z_near);
                        proj[3][2] = -(model.z_far * model.z_near) / (model.z_far - model.z_near);
                    } else {
                        proj[2][2] = (model.z_far + model.z_near) / (model.z_far - model.z_near);
                        proj[3][2] = -(2.0f * model.z_far * model.z_near) / (model.z_far - model.z_near);
                    }
                    if (ndc_target.y_direction == NDCYDirection::UP) {
                        proj[1][1] *= -1.0f; proj[2][1] *= -1.0f;
                    }
                }
                return proj;
            }, cam.model);

            runtime.cached_ndc_target = ndc_target;
            cam.intrinsic_dirty = false; // Clear the flag!
        }

        // Compute final updated VP matrix
        runtime.view_projection_matrix = runtime.projection_matrix * runtime.view_matrix;
    }

}



// --- Lifecycle ---------------------------------------------------
entt::entity SpawnCamera(entt::registry& registry, const CameraModelVariant& model, rhi::RenderTargetHandle target_fbo) {

    // Ensure no other camera points to the same target_fbo (ignoring TARGET_NULL)
    auto view = registry.view<CameraComponent>();
    for (auto e : view) {
        auto& cam = view.get<CameraComponent>(e);
        if (cam.target_fbo == rhi::TARGET_NULL) continue;
        
        // Null other camera FBO (it stops rendering)
        if (cam.target_fbo == target_fbo) {
            cam.target_fbo = rhi::TARGET_NULL;  
        }
    }
    
    // Add default root transform
    entt::entity entity = registry.create();
    tf::AddTransform(registry, entity);
    
    // intrinsic_dirty is true by default in the struct.
    registry.emplace<CameraComponent>(entity, model, target_fbo, true);
    
    return entity;
}
void DestroyCamera(entt::registry& registry, entt::entity cam_entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraComponent, "DestroyCamera failed: Entity lacks a CameraComponent!");
    if (registry.valid(cam_entity)) {
        registry.destroy(cam_entity);
    }
}
void DestroyAllCameras(entt::registry& registry) {
    // Copy to avoid iterator invalidation during destruction 
    std::vector<entt::entity> cameras = GetAllCameras(registry);
    for (auto e : cameras) {
        DestroyCamera(registry, e);
    }
}



// --- Setters -----------------------------------------------------
void SetCameraModel(entt::registry& registry, entt::entity cam_entity, const CameraModelVariant& model) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraComponent, "SetCameraModel failed: Entity lacks a CameraComponent!");
    
    auto& cam = registry.get<CameraComponent>(cam_entity);
    cam.model = model;
    cam.intrinsic_dirty = true; // Triggers the projection rebuild on the next tick
}
void SetPrimaryCamera(entt::registry& registry, entt::entity cam_entity) {
    SetCameraTarget(registry, cam_entity, rhi::TARGET_MAIN);
}
void SetCameraTarget(entt::registry& registry, entt::entity cam_entity, rhi::RenderTargetHandle target_fbo) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraComponent, "SetCameraTarget failed: Entity lacks a CameraComponent!");
    
    // Ensure no other camera points to the same target_fbo (ignoring TARGET_NULL)
    auto view = registry.view<CameraComponent>();
    for (auto e : view) {
        auto& cam = view.get<CameraComponent>(e);
        if (e == cam_entity) {
            cam.target_fbo = target_fbo;        // Set the target entity to draw to the screen
        } else if (cam.target_fbo == target_fbo) {
            cam.target_fbo = rhi::TARGET_NULL;  // If any other camera was drawing to the same target, strip its target
        }
    }
}
void SetCameraPositionAndLookAt(entt::registry& registry, entt::entity cam_entity, const glm::vec3& pos, const glm::vec3& target, const glm::vec3& world_up) {
    glm::quat rotation = CalculateLookAtRotation(pos, target, world_up);
    tf::SetLocalPosition(registry, cam_entity, pos);
    tf::SetLocalRotation(registry, cam_entity, rotation);
}
void SetCameraLookAt(entt::registry& registry, entt::entity cam_entity, const glm::vec3& target, const glm::vec3& world_up) {
    glm::vec3 current_pos = tf::GetLocalPosition(registry, cam_entity);
    glm::quat rotation = CalculateLookAtRotation(current_pos, target, world_up);
    tf::SetLocalRotation(registry, cam_entity, rotation);
}



// --- Getters -----------------------------------------------------
std::vector<entt::entity> GetAllCameras(entt::registry& registry) {
    auto view = registry.view<CameraComponent>();
    return std::vector<entt::entity>(view.begin(), view.end());
}
CameraModelVariant GetCameraModel(entt::registry& registry, entt::entity cam_entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraComponent, "GetCameraModel failed: Entity lacks a CameraComponent!");
    return registry.get<CameraComponent>(cam_entity).model;
}
entt::entity GetPrimaryCamera(entt::registry& registry) {
    auto view = registry.view<CameraComponent>();
    for (auto entity : view) {
        if (view.get<CameraComponent>(entity).target_fbo == rhi::TARGET_MAIN) {
            return entity;
        }
    }
    return entt::null;
}
const glm::mat4& GetViewMatrix(entt::registry& registry, entt::entity cam_entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraRuntimeComponent, "GetViewMatrix failed: Entity lacks a CameraRuntimeComponent!");
    return registry.get<CameraRuntimeComponent>(cam_entity).view_matrix;
}
const glm::mat4& GetProjectionMatrix(entt::registry& registry, entt::entity cam_entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraRuntimeComponent, "GetProjectionMatrix failed: Entity lacks a CameraRuntimeComponent!");
    return registry.get<CameraRuntimeComponent>(cam_entity).projection_matrix;
}
const glm::mat4& GetViewProjectionMatrix(entt::registry& registry, entt::entity cam_entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, cam_entity, CameraRuntimeComponent, "GetViewProjectionMatrix failed: Entity lacks a CameraRuntimeComponent!");
    return registry.get<CameraRuntimeComponent>(cam_entity).view_projection_matrix;
}




} // namespace rrl::camera