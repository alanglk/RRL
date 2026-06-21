// RRL/include/data/ImageData.hpp
#pragma once


#include <cstddef>
#include <cstdint>
#include <vector>


namespace rrl::data {
    

/**
 * @brief Defines the underlying scalar type of the image pixels (Similar to CV_8U, CV_32F).
 */
enum class ImageDataType : uint8_t {
    UINT8   = 1, // Standard textures (0-255)
    UINT16  = 2, // High-res depth maps or raw sensor data
    FLOAT32 = 3  // HDR images, computed data matrices
};


/**
 * @brief Defines the raw memory layout (number of channels).
 */
enum class ImageChannelLayout : uint8_t {
    CH_1 = 1,   // Grayscale, Depth, Binary Masks
    CH_2 = 2,   // Optical Flow (X,Y), Complex Math (Real, Imag), UVs
    CH_3 = 3,   // Standard Color
    CH_4 = 4    // Color + Alpha
};


/**
 * @brief Defines the semantic meaning of the channels (Color Space).
 */
enum class ImageColorLayout : uint8_t {
    NONE    = 0, // Used for pure math/data matrices (Depth, Flow)
    GRAY    = 1, 
    RGB     = 2, 
    BGR     = 3, // OpenCV standard
    RGBA    = 4, 
    BGRA    = 5,
    HSV     = 6  // HSV color space (hue, saturation, value)
};


/**
 * @brief Image data container. Holds metadata and a raw contiguous byte payload.
 */
struct ImageData {
    uint32_t width {0};             // Image width
    uint32_t height {0};            // Image height
    ImageChannelLayout channels;    // Image number of channels
    ImageDataType data_type;        // Image payload data type
    ImageColorLayout color_layout;  // Image color space layout (defines the stride)
    
    
    // Image raw data blob container
    std::vector<uint8_t> data; 
    size_t GetDataSize() const { return data.size(); }
};


} // namespace rrl::data