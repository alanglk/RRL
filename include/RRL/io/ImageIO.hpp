// RRL/include/io/ImageIO.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/asset/ImageAsset.hpp"
#include <string>


namespace rrl::io {


/**
 * @brief Loads an image from a file.
 * Automatically detects HDR (float32), 16-bit, or standard 8-bit formats.
 * Returns an ImageAsset struct. On failure, the `.data` vector will be empty.
 */
rrl::asset::ImageAsset RRL_API LoadImage(const std::string& filepath);


/**
 * @brief Loads an image from a raw memory buffer.
 */
rrl::asset::ImageAsset RRL_API LoadImage(const uint8_t* file_data, size_t data_size);


/**
 * @brief Saves an image to disk. The format is automatically inferred from the file extension 
 * (.png, .jpg, .bmp, .tga, .hdr). 
 * Returns true if successful.
 */
bool RRL_API SaveImage(const std::string& filepath, const rrl::asset::ImageAsset& image);


} // namespace rrl::io