// RRL/src/include/RRL/camera/CameraComponents.hpp
#pragma once

#include "RRL/camera/CameraConventions.hpp"
#include "RRL/camera/CameraModels.hpp"

#include "RRL/rhi/RHITypes.hpp"


namespace rrl::camera {
    

/**
 * @brief Camera component. It holds the camera model and 
 * it is the base source for computing the camera runtime matrices.
 */
struct CameraComponent {
    CameraModelVariant model { PerspectiveModel{} };
    // Flag to know if the camera intrinsics have changed (projection computation)
    bool intrinsic_dirty { true };

    // Flag to tell the RHI which camera should render onto the main screen buffer. 
    // THere can only be just one camera with its target pointing to  rhi::TARGET_SCREEN at a time
    rhi::RenderTargetHandle target_fbo {rhi::TARGET_MAIN};
    
    
    // What layers can this camera see?
    rhi::RHIRenderLayer culling_mask { rhi::RHIRenderLayer::LAYER_ALL };

    // This specifies the camera render priority:
    //      Low value   -> Higher priority
    //      High value  -> Lower priority
    uint32_t render_priority = 0; 
};


/**
 * @brief This is the actual runtime camera data source. 
 * It holds the pre-computed VP matrices so the RHI does not 
 * have to compute them every frame.
 */
struct CameraRuntimeComponent {
    glm::mat4 view_matrix { 1.0f };
    glm::mat4 projection_matrix { 1.0f };
    glm::mat4 view_projection_matrix { 1.0f };
    
    // Last used NDC convention to compute the runtime matrices
    NDCConvention cached_ndc_target;
    
    // Last used TF version to compute the runtime matrices
    uint32_t cached_tf_version { 0xFFFFFFFF }; 
};


/**
 * @brief Camera system general runtime cache.
 */
struct CameraCache {
    bool priority_dirty { false };  // Flag to know if any camera priority has changed (reordering).
    uint32_t next_priority { 0 };   // Auto-incrementing counter for default spawn priority
};


} // namespace rrl::camera