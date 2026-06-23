
#include "RRL/io/ImageIO.hpp"
#include <FLogging/FLogging.hpp>
#include <filesystem>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


using namespace rrl::data;
namespace fs = std::filesystem;


namespace rrl::io {
    

// Helper to simply deduce the channel and color layout from the number of channels (defaults)
static void DeduceLayouts(int channels, ImageChannelLayout& out_ch, ImageColorLayout& out_color) {
    switch(channels) {
        case 1: out_ch = ImageChannelLayout::CH_1; out_color = ImageColorLayout::GRAY; break;
        case 2: out_ch = ImageChannelLayout::CH_2; out_color = ImageColorLayout::NONE; break;
        case 3: out_ch = ImageChannelLayout::CH_3; out_color = ImageColorLayout::RGB;  break;
        case 4: out_ch = ImageChannelLayout::CH_4; out_color = ImageColorLayout::RGBA; break;
        default: out_ch = ImageChannelLayout::CH_4; out_color = ImageColorLayout::RGBA; break;
    }
}


// --- I/O ---------------------------------------------------------
ImageData LoadImage(const std::string& filepath) {
    ImageData img;

    // Check filepath
    if(!fs::exists(filepath)) {
        LOG_ERROR("LoadImage failed. Image path '{}' does not exist.", filepath);
        return img;
    }


    // Read image metadata
    int width = 0, height = 0, channels = 0;
    if (!stbi_info(filepath.c_str(), &width, &height, &channels)) {
        LOG_ERROR("LoadImage failed. Unable to load image metadata from '{}'", filepath);
        return img;
    }

    img.width = width;
    img.height = height;
    DeduceLayouts(channels, img.channels, img.color_layout);

    // Dynamic Factory Detection
    if (stbi_is_hdr(filepath.c_str())) {
        // HDR / Environment Maps
        img.data_type = ImageDataType::FLOAT32;
        float* raw_data = stbi_loadf(filepath.c_str(), &width, &height, &channels, 0);
        
        if (raw_data) {
            size_t bytes = width * height * channels * sizeof(float);
            img.data.assign(reinterpret_cast<uint8_t*>(raw_data), reinterpret_cast<uint8_t*>(raw_data) + bytes);
            stbi_image_free(raw_data);
        }
    } 
    else if (stbi_is_16_bit(filepath.c_str())) {
        // 16-Bit Maps (like high precision heightmaps)
        img.data_type = ImageDataType::UINT16;
        uint16_t* raw_data = stbi_load_16(filepath.c_str(), &width, &height, &channels, 0);
        
        if (raw_data) {
            size_t bytes = width * height * channels * sizeof(uint16_t);
            img.data.assign(reinterpret_cast<uint8_t*>(raw_data), reinterpret_cast<uint8_t*>(raw_data) + bytes);
            stbi_image_free(raw_data);
        }
    } 
    else {
        // Standard 8-bit Textures (PNG, JPG)
        img.data_type = ImageDataType::UINT8;
        uint8_t* raw_data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
        
        if (raw_data) {
            size_t bytes = width * height * channels * sizeof(uint8_t);
            img.data.assign(raw_data, raw_data + bytes);
            stbi_image_free(raw_data);
        }
    }

    return img;
}


ImageData LoadImage(const uint8_t* file_data, size_t data_size) {
    ImageData img;
    int width = 0, height = 0, channels = 0;

    if (!stbi_info_from_memory(file_data, static_cast<int>(data_size), &width, &height, &channels)) {
        LOG_ERROR("LoadImage failed. Unable to load image metadata from binary blob.");
        return img; 
    }

    img.width = width;
    img.height = height;
    DeduceLayouts(channels, img.channels, img.color_layout);

    if (stbi_is_hdr_from_memory(file_data, static_cast<int>(data_size))) {
        img.data_type = ImageDataType::FLOAT32;
        float* raw_data = stbi_loadf_from_memory(file_data, static_cast<int>(data_size), &width, &height, &channels, 0);
        if (raw_data) {
            size_t bytes = width * height * channels * sizeof(float);
            img.data.assign(reinterpret_cast<uint8_t*>(raw_data), reinterpret_cast<uint8_t*>(raw_data) + bytes);
            stbi_image_free(raw_data);
        }
    } else {
        // Defaulting to 8-bit for generic memory streams
        img.data_type = ImageDataType::UINT8;
        uint8_t* raw_data = stbi_load_from_memory(file_data, static_cast<int>(data_size), &width, &height, &channels, 0);
        if (raw_data) {
            size_t bytes = width * height * channels * sizeof(uint8_t);
            img.data.assign(raw_data, raw_data + bytes);
            stbi_image_free(raw_data);
        }
    }

    return img;
}


bool SaveImage(const std::string& filepath, const ImageData& image) {
    if (image.data.empty()) {
        LOG_ERROR("SaveImage failed. Received empty image.");
        return false;
    }

    // Extract extension and convert to lowercase
    fs::path path(filepath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    int ch = static_cast<int>(image.channels);
    int stride = image.width * ch * ((image.data_type == ImageDataType::FLOAT32) ? 4 : (image.data_type == ImageDataType::UINT16) ? 2 : 1);

    if (ext == ".png") {
        return stbi_write_png(filepath.c_str(), image.width, image.height, ch, image.data.data(), stride) != 0;
    } 
    else if (ext == ".jpg" || ext == ".jpeg") {
        return stbi_write_jpg(filepath.c_str(), image.width, image.height, ch, image.data.data(), 90) != 0; // 90 Quality
    } 
    else if (ext == ".bmp") {
        return stbi_write_bmp(filepath.c_str(), image.width, image.height, ch, image.data.data()) != 0;
    } 
    else if (ext == ".tga") {
        return stbi_write_tga(filepath.c_str(), image.width, image.height, ch, image.data.data()) != 0;
    } 
    else if (ext == ".hdr" && image.data_type == ImageDataType::FLOAT32) {
        return stbi_write_hdr(filepath.c_str(), image.width, image.height, ch, reinterpret_cast<const float*>(image.data.data())) != 0;
    }
    
    
    LOG_ERROR("SaveImage failed. Unsupported format or datatype mismatch.");
    return false; 
}



} // namespace rrl::io
