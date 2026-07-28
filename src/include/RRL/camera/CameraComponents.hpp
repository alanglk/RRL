// RRL/src/include/RRL/camera/CameraComponents.hpp
#pragma once

#include "RRL/camera/CameraConventions.hpp"
#include "RRL/camera/CameraModels.hpp"

#include "RRL/rhi/RHIBackend.hpp"


namespace rrl::camera {
    

enum class CameraBackgroundMode: uint8_t {
    DEFAULT_SCENE_ENVIRONMENT,  // Default, uses the global scene environment HDRI/Color
    OVERRIDE_SOLID_COLOR,       // Ignores scene, uses custom clear color
    OVERRIDE_FLAT_TEXTURE       // Ignores scene, renders a 2D texture as a flat screen background
};


/**
 * @brief Camera component. It holds the camera model and 
 * it is the base source for computing the camera runtime matrices.
 */
struct CameraComponent {
    CameraModelVariant model { PerspectiveModel{} };

    // Defines the CS of this camera
    CameraViewBasis view_basis = CameraViewBasis::STANDARD_OPENGL; 

    // Flag to know if the camera intrinsics have changed (projection computation)
    bool intrinsic_dirty { true };
    
    // What layers can this camera see?
    rhi::RHIRenderLayerMask culling_mask { rhi::RHIRenderLayerMask::LAYER_ALL };

    // This specifies the camera render priority:
    //      Low value   -> Higher priority
    //      High value  -> Lower priority
    uint32_t render_priority = 0; 


    // Camera background overrides
    CameraBackgroundMode bg_mode        = CameraBackgroundMode::DEFAULT_SCENE_ENVIRONMENT;
    glm::vec4 bg_override_clear_color   = {0.0f, 0.0f, 0.0f, 1.0f};
    rrl::AssetID bg_override_texture    = rrl::NULL_ASSET; 
};

/**
 * @brief Maps a camera to a Render Target. 
 * [THIS WILL BE DEPRECATED ON FUTURE VERSIONS]
 * [Transition to RDG]: Currently, the RHI iterates over entities with this 
 * component to dispatch draw calls. Once the Render Dependency Graph (RDG) 
 * is implemented, the RDG will define passes dynamically, and this component 
 * will be deprecated and removed.
 */
struct CameraOutputComponent {
    rhi::RenderTargetHandle target_fbo { rhi::BACKEND_TARGET_MAIN };
};


/**
 * @brief This is the actual runtime camera data source. 
 * It holds the pre-computed VP matrices so the RHI does not 
 * have to compute them every frame. This is instantiated by
 * the UpdateCameras system.
 */
struct CameraRuntimeComponent {
    glm::mat4 view_matrix { 1.0f };
    glm::mat4 projection_matrix { 1.0f };
    glm::mat4 view_projection_matrix { 1.0f };
    
    // Last used NDC convention to compute the runtime matrices
    NDCConvention cached_ndc_target;
    
    // Last used TF version to compute the runtime matrices
    uint32_t cached_tf_version { 0xFFFFFFFF }; 
    
    // Holds the texure handle in CameraBackgroundMode::OVERRIDE_FLAT_TEXTURE mode
    rhi::TextureHandle resolved_bg_texture { rhi::BACKEND_TEXTURE_NULL };
};


/**
 * @brief Camera system general runtime cache.
 */
struct CameraCache {
    bool priority_dirty { false };  // Flag to know if any camera priority has changed (reordering).
    uint32_t next_priority { 0 };   // Auto-incrementing counter for default spawn priority
};


} // namespace rrl::camera