// RRL/include/camera/CameraConventions.hpp
#pragma once

#include <cstdint>

namespace rrl::camera {
    
/**
 * @brief Defines the depth (Z-axis) clipping range for Normalized Device Coordinates (NDC).
 */
enum class NDCDepth : uint8_t { 
    ZERO_TO_ONE,        // [ 0, 1]  - Used by WebGPU, Vulkan, DirectX, and OpenCV
    MINUS_ONE_TO_ONE    // [-1, 1]  - Used by standard OpenGL
};

/**
 * @brief Defines the vertical (Y-axis) orientation for Normalized Device Coordinates (NDC).
 */
enum class NDCYDirection : uint8_t { 
    UP,                 // +Y points Up   - Used by OpenGL, WebGPU
    DOWN                // +Y points Down - Used by Vulkan, OpenCV
};

/**
 * @brief Configuration profile representing the specific NDC requirements of a RHI.
 *  This is used by the automatic camera system to adapt the generated projection 
 *  matrices to the active rendering backend.
 */
struct NDCConvention {
    NDCDepth depth_range;
    NDCYDirection y_direction;

    // Comparison operators
    bool operator==(const NDCConvention& other) const { 
        return depth_range == other.depth_range && y_direction == other.y_direction; 
    }
    bool operator!=(const NDCConvention& other) const { 
        return !(*this == other); 
    }
};



// TODO: This will be defined inside each RHI !!!
constexpr NDCConvention NDC_OPENGL = { NDCDepth::MINUS_ONE_TO_ONE, NDCYDirection::UP };
constexpr NDCConvention NDC_WEBGPU = { NDCDepth::ZERO_TO_ONE,      NDCYDirection::UP };
constexpr NDCConvention NDC_OPENCV = { NDCDepth::ZERO_TO_ONE,      NDCYDirection::DOWN };



} // namespace rrl::camera