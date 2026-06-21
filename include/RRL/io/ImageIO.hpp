// RRL/include/io/ImageIO.hpp
#pragma once

#include "RRL/data/ImageData.hpp"
#include <string>


namespace rrl::io {


/**
 * @brief Loads an image from a file.
 * Automatically detects HDR (float32), 16-bit, or standard 8-bit formats.
 * Returns an ImageData struct. On failure, the `.data` vector will be empty.
 */
data::ImageData LoadImage(const std::string& filepath);


/**
 * @brief Loads an image from a raw memory buffer.
 */
data::ImageData LoadImage(const uint8_t* file_data, size_t data_size);


/**
 * @brief Saves an image to disk. The format is automatically inferred from the file extension 
 * (.png, .jpg, .bmp, .tga, .hdr). 
 * Returns true if successful.
 */
bool SaveImage(const std::string& filepath, const data::ImageData& image);


} // namespace rrl::io