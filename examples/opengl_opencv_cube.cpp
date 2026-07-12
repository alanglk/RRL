// examples/opengl_glfw_cube.cpp

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <thread>
#include <chrono>

#include <FLogging/FLogging.hpp>


// RRL Engine Modules
#include <RRL/RRLEngine.hpp>


// Helper Function to Create a 3D Cube
rrl::asset::MeshAsset CreateWireframeCube(float size) {
    rrl::asset::MeshAsset mesh;
    mesh.topology = rrl::asset::MeshTopology::TRIANGLES;
    float h = size * 0.5f;

    // 8 Corner Vertices of a Cube (ISO 8855 Coordinate System)
    mesh.positions = {
        {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h}, // Bottom 4 corners
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}  // Top 4 corners
    };
    
    mesh.indices = {
        0, 2, 1,  0, 3, 2, // Bottom face
        4, 5, 6,  4, 6, 7, // Top face 
        0, 1, 5,  0, 5, 4, // Front face 
        1, 2, 6,  1, 6, 5, // Right face 
        2, 3, 7,  2, 7, 6, // Back face 
        3, 0, 4,  3, 4, 7  // Left face 
    };

    mesh.submeshes = {{0, static_cast<uint32_t>(mesh.indices.size())}};
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

    // Initialize Engine
    LOG_INFO("[RRL Engine] Booting engine...");
    rrl::Engine engine;

    auto window = engine.rhi.CreateWindow(rrl::rhi::RHIWindowType::OPENCV);
    engine.rhi.InitializeWindow(window, "RRL - 3D Mesh + 2D UI OPENGL", window_w, window_h);
    engine.rhi.LoadBackend(rrl::rhi::RHIBackendType::OPENGL);
    engine.rhi.Initialize(&window);


    // 3D Cube Creation
    rrl::asset::MaterialAsset unlit_blue;
    unlit_blue.shading_model    = rrl::asset::ShadingModel::UNLIT;
    unlit_blue.base_color       = glm::vec4(0.2f, 0.6f, 1.0f, 1.0f); 
    rrl::AssetID blue_mat       = engine.asset.CreateMaterial("blue_mat", unlit_blue);
    rrl::AssetID cube_mesh      = engine.asset.CreateMesh("cube_mesh", CreateWireframeCube(2.0f));
    
    rrl::ObjectID cube_obj      = engine.scene.SpawnObject();
    engine.tf.AddTransform(cube_obj, glm::vec3(0.0f, 0.0f, 0.0f));
    engine.asset.BindMesh(cube_obj, cube_mesh, {blue_mat});


    // 2D UI Object Creation
    rrl::asset::ImageAsset ui_image; // image model
    ui_image.width          = ui_w;
    ui_image.height         = ui_h;
    ui_image.channels       = rrl::asset::ImageChannelLayout::CH_3;
    ui_image.color_layout   = rrl::asset::ImageColorLayout::RGB;
    ui_image.data_type      = rrl::asset::ImageAssetType::UINT8;
    rrl::AssetID ui_tex     = engine.asset.CreateTexture("ui_texture", std::move(ui_image));
    rrl::ObjectID ui_obj    = engine.scene.SpawnObject();
    engine.asset.BindUITexture(ui_obj, ui_tex, 0.02f, 0.02f, 0.25f, 0.25f);


    // Camera Setup
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians  = glm::radians(60.0f);
    camera_model.aspect_ratio   = float(window.width) / float(window.height);
    camera_model.z_near         = 0.1f;
    camera_model.z_far          = 100.0f;
    rrl::ObjectID main_camera   = engine.camera.SpawnCamera(camera_model);
    engine.camera.SetCameraPositionAndLookAt(main_camera, {-6.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f});


    // Main loop
    LOG_INFO("[RRL Engine] Entering main loop...");
    float rotation_z = 0.0f;
    
    while (engine.rhi.PollWindowEvents(window)) {

        // 3D Cube Rotation Update
        rotation_z += 0.02f;
        engine.tf.SetLocalRotation(cube_obj, glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f)));

        // 2D UI Texture Update
        rrl::asset::ImageAsset dynamic_frame;
        dynamic_frame.width         = ui_w;
        dynamic_frame.height        = ui_h;
        dynamic_frame.channels      = rrl::asset::ImageChannelLayout::CH_3;
        dynamic_frame.color_layout  = rrl::asset::ImageColorLayout::RGB;
        dynamic_frame.data_type     = rrl::asset::ImageAssetType::UINT8;
        dynamic_frame.data.resize(ui_w * ui_h * 3);
        uint8_t shift = static_cast<uint8_t>((std::sin(rotation_z) * 0.5f + 0.5f) * 255.0f);
        std::fill(dynamic_frame.data.begin(), dynamic_frame.data.end(), shift);
        engine.asset.UpdateTexture(ui_tex, std::move(dynamic_frame));


        // Tick Engine Logic
        engine.tf.UpdateTransformTree();
        engine.camera.UpdateCameras(rrl::camera::NDC_OPENGL);
        engine.rhi.RenderFrame();
        
        // ~60 FPS main loop
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup (engine handles its shutdown on its destructor)
    LOG_INFO("[RRL Engine] Shutting down...");
    flogging::ResetLogger();
    return 0;
}

