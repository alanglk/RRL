// RRL/include/data/ImageAsset.hpp
#pragma once

#include <RRL/rrl_export.h>

#include <cstddef>
#include <cstdint>
#include <vector>


namespace rrl::asset {
    

/**
 * @brief Defines the underlying scalar type of the image pixels (Similar to CV_8U, CV_32F).
 */
enum class ImageAssetType : uint8_t {
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

enum class ImageOrigin {
    TOP_LEFT,       // Standard (OpenCV, STB Image, Vulkan, UI)
    BOTTOM_LEFT     // OpenGL Native FBOs
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
 * @brief Defines the algorithm for texture upsacling and downscaling.
 */
enum class ImageFilter {
    LINEAR,
    NEAREST
};


/**
 * @brief Image data container. Holds metadata and a raw contiguous byte payload.
 */
struct RRL_API ImageAsset {
    uint32_t width                  { 0 };                          // Image width
    uint32_t height                 { 0 };                          // Image height
    ImageChannelLayout channels     { ImageChannelLayout::CH_1 };   // Image number of channels
    ImageAssetType data_type         { ImageAssetType::FLOAT32 };     // Image payload data type
    ImageColorLayout color_layout   { ImageColorLayout::NONE };     // Image color space layout (defines the stride)
    ImageOrigin origin              { ImageOrigin::TOP_LEFT };      // where does the data start from?
    ImageFilter filter              { ImageFilter::LINEAR };        // How to sample / scale the image
    
    
    // Image raw data blob container
    std::vector<uint8_t> data = { }; 
    size_t GetDataSize() const { return data.size(); }
    
    /**
     * @brief Checks if the image is valid (propper metadata and contains actual data).
     * It validates dimensions, enum bounds, and buffer sizing.
     */
    bool IsValid() const {
        // Dimensions must be non-zero
        if (width == 0 || height == 0 || data.empty()) return false;

        // Resolve channel count
        size_t channel_count = 0;
        switch (channels) {
            case ImageChannelLayout::CH_1: channel_count = 1; break;
            case ImageChannelLayout::CH_2: channel_count = 2; break;
            case ImageChannelLayout::CH_3: channel_count = 3; break;
            case ImageChannelLayout::CH_4: channel_count = 4; break;
            default: return false; // Invalid channel enum
        }

        // Resolve bytes per channel
        size_t bytes_per_channel = 0;
        switch (data_type) {
            case ImageAssetType::UINT8:   bytes_per_channel = 1; break;
            case ImageAssetType::UINT16:  bytes_per_channel = 2; break;
            case ImageAssetType::FLOAT32: bytes_per_channel = 4; break;
            default: return false; // Invalid data type enum
        }

        // Validate color layout enum
        if (static_cast<uint8_t>(color_layout) > 6) return false;

        // Expected footprint
        size_t expected_size = static_cast<size_t>(width) * height * channel_count * bytes_per_channel;
        return data.size() == expected_size;
    }
};


} // namespace rrl::asset