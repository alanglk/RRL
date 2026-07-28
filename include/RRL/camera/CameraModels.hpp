// RRL/include/camera/CameraModels.hpp
#pragma once

#include <RRL/rrl_export.h>

#include <glm/glm.hpp>
#include <variant>
#include <vector>
#include <cstdint>


namespace rrl::camera {


struct RRL_API PerspectiveModel {
    float fov_y_radians { glm::radians(60.0f) };
    float aspect_ratio  { 16.0f / 9.0f };
    float z_near        { 0.1f };
    float z_far         { 1000.0f };
};


struct RRL_API OrthographicModel {
    float width  { 10.0f };
    float height { 10.0f };
    float z_near { -100.0f };
    float z_far  { 100.0f };
};


struct RRL_API PinholeModel {
    float fx { 500.0f };
    float fy { 500.0f };
    float cx { 320.0f };
    float cy { 240.0f };
    uint32_t width_px  { 640 };
    uint32_t height_px { 480 };
    float z_near { 0.1f };
    float z_far  { 1000.0f };

    // Left empty for an ideal pinhole; populated for distorted lenses.
    std::vector<float> distortion_coeffs; // e.g., OpenCV k1, k2, p1, p2
};


using CameraModelVariant = std::variant<PerspectiveModel, OrthographicModel, PinholeModel>;


} // namespace rrl::camera



