// RRL/src/camera/CameraSystem.cpp

#include "RRL/camera/CameraSystem.hpp"
#include "RRL/camera/CameraComponents.hpp"
#include "RRL/tf/TFComponents.hpp"

#include <FLogging/FLogging.hpp>

#include "RRL/DebugMacros.hpp"


namespace rrl::camera {
    

// Matrix to map ISO 8855 (X-front, Y-left, Z-up) 
//  to OpenCV CCS (X-right, Y-down, Z-front)
constexpr glm::mat4 LCS_TO_CCS = glm::mat4(
    0.0f,  0.0f,  1.0f,  0.0f,
   -1.0f,  0.0f,  0.0f,  0.0f,
    0.0f, -1.0f,  0.0f,  0.0f,
    0.0f,  0.0f,  0.0f,  1.0f
);


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
void AddCamera(entt::registry& registry, entt::entity entity, const CameraModelVariant& model, bool is_primary) {
    RRL_ASSERT_NOT_HAS_COMPONENT(registry, entity, CameraComponent, "AddCamera failed: Entity already has a CameraComponent!");

    // If this is set to primary, ensure no other camera holds the primary flag
    if (is_primary) {
        auto view = registry.view<CameraComponent>();
        for (auto e : view) {
            view.get<CameraComponent>(e).is_primary = false;
        }
    }

    // intrinsic_dirty is true by default in the struct.
    registry.emplace<CameraComponent>(entity, model, is_primary, true);
}

void RemoveCamera(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraComponent, "RemoveCamera failed: Entity lacks a CameraComponent!");
    
    registry.remove<CameraComponent>(entity);
    
    // Clean up the runtime cache so we don't leak memory or leave stale matrix data
    if (registry.all_of<CameraRuntimeComponent>(entity)) {
        registry.remove<CameraRuntimeComponent>(entity);
    }
}



// --- Setters -----------------------------------------------------
void SetCameraModel(entt::registry& registry, entt::entity entity, const CameraModelVariant& model) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraComponent, "SetCameraModel failed: Entity lacks a CameraComponent!");
    
    auto& cam = registry.get<CameraComponent>(entity);
    cam.model = model;
    cam.intrinsic_dirty = true; // Triggers the projection rebuild on the next tick
}
void SetPrimaryCamera(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraComponent, "SetPrimaryCamera failed: Entity lacks a CameraComponent!");
    
    auto view = registry.view<CameraComponent>();
    for (auto e : view) {
        // Only evaluates to true if the current entity matches the target entity
        view.get<CameraComponent>(e).is_primary = (e == entity);
    }
}



// --- Getters -----------------------------------------------------
CameraModelVariant GetCameraModel(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraComponent, "GetCameraModel failed: Entity lacks a CameraComponent!");
    return registry.get<CameraComponent>(entity).model;
}
entt::entity GetPrimaryCamera(entt::registry& registry) {
    auto view = registry.view<CameraComponent>();
    for (auto entity : view) {
        if (view.get<CameraComponent>(entity).is_primary) {
            return entity;
        }
    }
    return entt::null;
}
const glm::mat4& GetViewMatrix(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraRuntimeComponent, "GetViewMatrix failed: Entity lacks a CameraRuntimeComponent!");
    return registry.get<CameraRuntimeComponent>(entity).view_matrix;
}
const glm::mat4& GetProjectionMatrix(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraRuntimeComponent, "GetProjectionMatrix failed: Entity lacks a CameraRuntimeComponent!");
    return registry.get<CameraRuntimeComponent>(entity).projection_matrix;
}
const glm::mat4& GetViewProjectionMatrix(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, CameraRuntimeComponent, "GetViewProjectionMatrix failed: Entity lacks a CameraRuntimeComponent!");
    return registry.get<CameraRuntimeComponent>(entity).view_projection_matrix;
}




} // namespace rrl::camera