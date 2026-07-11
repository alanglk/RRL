// RRL/src/include/RRL/camera/CameraComponents.hpp
#pragma once

#include "RRL/camera/CameraConventions.hpp"
#include "RRL/camera/CameraModels.hpp"

#include "RRL/rhi/RHIBackend.hpp"


namespace rrl::camera {
    

/**
 * @brief Camera component. It holds the camera model and 
 * it is the base source for computing the camera runtime matrices.
 */
struct CameraComponent {
    CameraModelVariant model { PerspectiveModel{} };
    // Flag to know if the camera intrinsics have changed 
    bool intrinsic_dirty { true };

    // Flag to tell the RHI which camera should render onto the main screen buffer. 
    // THere can only be just one camera with its target pointing to  rhi::TARGET_SCREEN at a time
    rhi::RenderTargetHandle target_fbo {rhi::TARGET_MAIN};
    
    
    // What layers can this camera see?
    rhi::RHIRenderLayer culling_mask { rhi::RHIRenderLayer::LAYER_ALL };
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


} // namespace rrl::camera