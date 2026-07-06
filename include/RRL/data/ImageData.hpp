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
 * @brief Image data container. Holds metadata and a raw contiguous byte payload.
 */
struct ImageData {
    uint32_t width                  { 0 };                          // Image width
    uint32_t height                 { 0 };                          // Image height
    ImageChannelLayout channels     { ImageChannelLayout::CH_1 };   // Image number of channels
    ImageDataType data_type         { ImageDataType::FLOAT32 };     // Image payload data type
    ImageColorLayout color_layout   { ImageColorLayout::NONE };     // Image color space layout (defines the stride)
    ImageOrigin origin              { ImageOrigin::TOP_LEFT };      // where does the data start from?
    
    
    // Image raw data blob container
    std::vector<uint8_t> data = { }; 
    size_t GetDataSize() const { return data.size(); }
    
    /**
     * @brief Checks if the image model is properly initialized.
     * It validates dimensions, enum bounds, and buffer sizing (if data is populated).
     */
    bool IsImageModelValid() const {
        // Dimensions must be non-zero
        if (width == 0 || height == 0) return false;

        // Validate enums (protects against static_cast garbage)
        bool valid_type = (data_type == ImageDataType::UINT8 || data_type == ImageDataType::UINT16 || data_type == ImageDataType::FLOAT32);
        bool valid_channels = (static_cast<uint8_t>(channels) >= 1 && static_cast<uint8_t>(channels) <= 4);
        bool valid_color = (static_cast<uint8_t>(color_layout) <= 6);
        
        if (!valid_type || !valid_channels || !valid_color) return false;

        // If the data buffer is populated, its size must exactly match the metadata footprint
        if (!data.empty()) {
            size_t bytes_per_channel = 1;
            if (data_type == ImageDataType::UINT16) bytes_per_channel = 2;
            else if (data_type == ImageDataType::FLOAT32) bytes_per_channel = 4;

            size_t expected_size = static_cast<size_t>(width) * height * static_cast<uint8_t>(channels) * bytes_per_channel;
            if (data.size() != expected_size) return false;
        }

        return true;
    }
};


} // namespace rrl::data