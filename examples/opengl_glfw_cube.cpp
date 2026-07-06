// examples/opengl_glfw_cube.cpp

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <chrono>
#include <thread>

#include <FLogging/FLogging.hpp>

// RRL Engine Modules
#include "RRL/data/AssetManager.hpp"
#include "RRL/data/ImageData.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"
#include "RRL/data/MeshData.hpp"
#include "RRL/rhi/RHIAPI.hpp"
#include "entt/entity/fwd.hpp"


// Helper Function to Create a 3D Cube
rrl::data::MeshData CreateWireframeCube(float size, entt::entity material) {
    rrl::data::MeshData mesh;
    mesh.topology = rrl::data::MeshTopology::TRIANGLES;
    float h = size * 0.5f;

    // 8 Corner Vertices of a Cube (ISO 8855 Coordinate System)
    mesh.positions = {
        {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h}, // Bottom 4 corners
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}  // Top 4 corners
    };
    mesh.indices = {
        0, 1, 2,  0, 2, 3, // Bottom face
        4, 5, 6,  4, 6, 7, // Top face
        0, 1, 5,  0, 5, 4, // Front face
        1, 2, 6,  1, 6, 5, // Right face
        2, 3, 7,  2, 7, 6, // Back face
        3, 0, 4,  3, 4, 7  // Left face
    };

    mesh.materials.push_back({0, static_cast<uint32_t>(mesh.indices.size()), material});
    return mesh;
}


int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    LOG_INFO("[RRL Engine] Running 3D cube rendering on OpenCV Backend");

    uint32_t window_w       = 1280;
    uint32_t window_h       = 720;
    uint32_t ui_w           = 256;
    uint32_t ui_h           = 256;

    // Initialize Systems
    LOG_INFO("[RRL Engine] Booting subsystems...");
    entt::registry registry;
    rrl::tf::RegisterTFActions(registry);
    rrl::data::InitializeAssetManager(registry);
    rrl::rhi::RHIWindow main_window = rrl::rhi::CreateWindow(rrl::rhi::RHIWindowType::GLFW);
    rrl::rhi::InitializeWindow(main_window, "RRL - 3D Mesh + 2D UI", window_w, window_h);
    if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::OPENGL, registry)) {
        LOG_ERROR("[RRL Engine] Failed to load RHI backend!");
        return -1;
    }
    if (!rrl::rhi::Initialize(registry, &main_window)) {
        LOG_ERROR("[RRL Engine] Failed to initialize RHI backend!");
        return -1;
    }


    // 3D Cube Creation
    rrl::data::MaterialData unlit_blue;
    unlit_blue.shading_model = rrl::data::ShadingModel::UNLIT;
    unlit_blue.base_color = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f); 
    entt::entity cube_mat = rrl::data::CreateMaterial(registry, "blue_mat", unlit_blue);

    entt::entity cube_asset = rrl::data::CreateMesh(registry, "cube_mesh", CreateWireframeCube(2.0f, cube_mat));
    auto cube_obj = registry.create();
    rrl::tf::AddTransform(registry, cube_obj, glm::vec3(0.0f, 0.0f, 0.0f));
    rrl::data::BindMesh(registry, cube_obj, cube_asset);

    // 2D UI Object Creation
    rrl::data::ImageData ui_image;
    ui_image.width = ui_w;
    ui_image.height = ui_h;
    ui_image.channels = rrl::data::ImageChannelLayout::CH_3;
    ui_image.color_layout  = rrl::data::ImageColorLayout::RGB;
    ui_image.data_type = rrl::data::ImageDataType::UINT8;
    entt::entity ui_texture = rrl::data::CreateTexture(registry, "ui_image", std::move(ui_image));
    entt::entity ui_obj     = registry.create();
    rrl::data::BindUITexture(registry, ui_obj, ui_texture, 0.02f, 0.02f, 0.25f, 0.25f);

    // Camera Setup
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = float(main_window.width) / float(main_window.height);
    camera_model.z_near = 0.1f;
    camera_model.z_far = 100.0f;
    entt::entity main_camera = rrl::camera::SpawnCamera(registry, camera_model);
    rrl::camera::SetCameraPositionAndLookAt(registry, main_camera, {-6.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f});


    // Main loop
    LOG_INFO("[RRL Engine] Entering main loop...");
    float rotation_z = 0.0f;
    
    // PollWindowEvents returns false when the GLFW window 'X' is clicked
    while (rrl::rhi::PollWindowEvents(main_window)) {

        // 3D Cube Rotation Update
        rotation_z += 0.02f;
        rrl::tf::SetLocalRotation(registry, cube_obj, glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f)));

        // 2D UI Texture Update
        rrl::data::ImageData dynamic_frame;
        dynamic_frame.width         = ui_w;
        dynamic_frame.height        = ui_h;
        dynamic_frame.channels      = rrl::data::ImageChannelLayout::CH_3;
        dynamic_frame.color_layout  = rrl::data::ImageColorLayout::RGB;
        dynamic_frame.data_type     = rrl::data::ImageDataType::UINT8;
        dynamic_frame.data.resize(ui_w * ui_h * 3);
        uint8_t shift = static_cast<uint8_t>((std::sin(rotation_z) * 0.5f + 0.5f) * 255.0f);
        std::fill(dynamic_frame.data.begin(), dynamic_frame.data.end(), shift);
        rrl::data::UpdateTexture(registry, ui_texture, std::move(dynamic_frame));


        // Tick Engine Logic
        rrl::tf::UpdateTransformTree(registry);
        rrl::camera::UpdateCameras(registry, rrl::camera::NDC_OPENGL);
        rrl::rhi::SyncResources(registry);
        
        rrl::rhi::RenderFrame(registry);
        
        rrl::rhi::Present(registry); 
        

        // ~60 FPS main loop
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup
    LOG_INFO("[RRL Engine] Shutting down...");
    rrl::camera::DestroyAllCameras(registry);
    rrl::data::DestroyAllAssets(registry);
    rrl::rhi::Shutdown(registry);
    rrl::rhi::DestroyWindow(registry, main_window);
    flogging::ResetLogger();
    return 0;
}

