// examples/opencv_robot.cpp

#include <cmath>
#include <glm/glm.hpp>

#include <FLogging/FLogging.hpp>


// RRL Engine Modules
#include <RRL/RRLEngine.hpp>
#include <RRL/RRLTypes.hpp>
#include <RRL/io/PrefabIO.hpp>


static rrl::asset::MeshAsset GenerateCuboidMesh(const glm::vec3& dimensions) {
    rrl::asset::MeshAsset mesh;
    mesh.topology = rrl::asset::MeshTopology::TRIANGLES;

    float hx = dimensions.x * 0.5f;
    float hy = dimensions.y * 0.5f;
    float hz = dimensions.z * 0.5f;

    // Front Face (+Z)
    glm::vec3 n_front(0.0f, 0.0f, 1.0f);
    mesh.positions.insert(mesh.positions.end(), { {-hx, -hy, hz}, {hx, -hy, hz}, {hx, hy, hz}, {-hx, hy, hz} });
    mesh.normals.insert(mesh.normals.end(), 4, n_front);
    mesh.uvs.insert(mesh.uvs.end(), { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} });

    // Back Face (-Z)
    glm::vec3 n_back(0.0f, 0.0f, -1.0f);
    mesh.positions.insert(mesh.positions.end(), { {hx, -hy, -hz}, {-hx, -hy, -hz}, {-hx, hy, -hz}, {hx, hy, -hz} });
    mesh.normals.insert(mesh.normals.end(), 4, n_back);
    mesh.uvs.insert(mesh.uvs.end(), { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} });

    // Left Face (-X)
    glm::vec3 n_left(-1.0f, 0.0f, 0.0f);
    mesh.positions.insert(mesh.positions.end(), { {-hx, -hy, -hz}, {-hx, -hy, hz}, {-hx, hy, hz}, {-hx, hy, -hz} });
    mesh.normals.insert(mesh.normals.end(), 4, n_left);
    mesh.uvs.insert(mesh.uvs.end(), { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} });

    // Right Face (+X)
    glm::vec3 n_right(1.0f, 0.0f, 0.0f);
    mesh.positions.insert(mesh.positions.end(), { {hx, -hy, hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {hx, hy, hz} });
    mesh.normals.insert(mesh.normals.end(), 4, n_right);
    mesh.uvs.insert(mesh.uvs.end(), { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} });

    // Top Face (+Y)
    glm::vec3 n_top(0.0f, 1.0f, 0.0f);
    mesh.positions.insert(mesh.positions.end(), { {-hx, hy, hz}, {hx, hy, hz}, {hx, hy, -hz}, {-hx, hy, -hz} });
    mesh.normals.insert(mesh.normals.end(), 4, n_top);
    mesh.uvs.insert(mesh.uvs.end(), { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} });

    // Bottom Face (-Y)
    glm::vec3 n_bottom(0.0f, -1.0f, 0.0f);
    mesh.positions.insert(mesh.positions.end(), { {-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, -hy, hz}, {-hx, -hy, hz} });
    mesh.normals.insert(mesh.normals.end(), 4, n_bottom);
    mesh.uvs.insert(mesh.uvs.end(), { {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f} });

    // Generate indices for 6 faces * 2 triangles per face * 3 vertices per triangle = 36 indices
    mesh.indices.reserve(36);
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t v_idx = face * 4;
        
        // Triangle 1 (CCW)
        mesh.indices.push_back(v_idx);
        mesh.indices.push_back(v_idx + 1);
        mesh.indices.push_back(v_idx + 2);
        
        // Triangle 2 (CCW)
        mesh.indices.push_back(v_idx + 2);
        mesh.indices.push_back(v_idx + 3);
        mesh.indices.push_back(v_idx);
    }

    return mesh;
}
static rrl::asset::ImageAsset GenerateCheckerboardTexture(uint32_t width, uint32_t height, uint32_t tile_size) {
    rrl::asset::ImageAsset img;
    img.width = width;
    img.height = height;
    img.channels = rrl::asset::ImageChannelLayout::CH_3;
    img.color_layout = rrl::asset::ImageColorLayout::RGB;
    img.data_type = rrl::asset::ImageAssetType::UINT8;
    
    img.data.resize(width * height * 3);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            bool is_magenta = ((x / tile_size) % 2) == ((y / tile_size) % 2);
            size_t idx = (y * width + x) * 3;
            
            img.data[idx + 0] = is_magenta ? 255 : 0; // Red
            img.data[idx + 1] = 0;                    // Green
            img.data[idx + 2] = is_magenta ? 255 : 0; // Blue
        }
    }
    return img;
}
static rrl::asset::MeshAsset GenerateFrustumScreenMesh() {
    rrl::asset::MeshAsset quad;
    quad.topology = rrl::asset::MeshTopology::TRIANGLES;
    quad.positions = {{-0.8f, -0.45f, 0.0f}, { 0.8f, -0.45f, 0.0f}, { 0.8f,  0.45f, 0.0f}, {-0.8f,  0.45f, 0.0f}};
    quad.uvs = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    quad.indices = { 0, 1, 2, 2, 3, 0 };
    return quad;
}




/**
 * @brief High-level controller class for a simulated robot instance.
 * Automatically handles the preloading of its prefab blueprint, physical instantiation, 
 * attaching a perspective camera sensor, and linking the camera's visual feed 
 * to a 2D UI screen layout in the engine.
 */
class Robot {
private:
    static constexpr char robot_prefab_id[6] = "robot";

    static void PreloadRobotPrefab(rrl::Engine& engine) {
        // Build the Prefab structure
        rrl::io::IOPrefab robot_prefab; 
        robot_prefab.filepath = robot_prefab_id;

        // Load the robot's texture
        auto checkerboard = GenerateCheckerboardTexture(512, 512, 64);
        rrl::asset::TextureID virtual_tex_path = "robot_albedo";
        engine.asset.CreateTexture(virtual_tex_path, std::move(checkerboard));

        // Create the robot's materials
        rrl::io::IOMaterial mat;
        mat.name = "robot_chassis_mat";
        mat.material_parameters.base_color = glm::vec4(1.0f);
        mat.albedo_path = virtual_tex_path; 
        robot_prefab.materials.push_back(mat);

        // Create the robot's geometry
        rrl::io::IONode chassis_node;
        chassis_node.name = "chassis";
        chassis_node.mesh = GenerateCuboidMesh({3.0f, 2.0f, 1.5f});
        chassis_node.mesh.submeshes.push_back({
            0, // index offset
            static_cast<uint32_t>(chassis_node.mesh.indices.size()) // index count
        });
        chassis_node.submesh_material_names.push_back("robot_chassis_mat");

        robot_prefab.root_nodes.push_back(std::move(chassis_node));
        
        // Cache the blueprint through the Scene Module
        engine.scene.PreloadPrefabBlueprint(robot_prefab_id, std::move(robot_prefab));
    }

    // Robot private attribs 
    rrl::ObjectID robot_instance = rrl::NULL_OBJECT;
    rrl::ObjectID robot_cam      = rrl::NULL_OBJECT;
    rrl::ObjectID ui_screen      = rrl::NULL_OBJECT;
    rrl::AssetID  fbo_tex        = rrl::NULL_ASSET;
    rrl::rhi::RenderTargetHandle cam_target = rrl::rhi::TARGET_NULL;
    
public:
    Robot() = default;
    
    rrl::ObjectID GetInstance() const { return robot_instance; }

    void Initialize(rrl::Engine& engine, 
        uint32_t robot_cam_width, uint32_t robot_cam_height,
        glm::vec3 pos = {0.0f, 0.0f, 0.0f}, 
        glm::quat rot = {1.0f, 0.0f, 0.0f, 0.0f},
        glm::vec3 scale = {1.0f, 1.0f, 1.0f}
    ) {
    
        // Preload and Spawn Robot
        PreloadRobotPrefab(engine);
        robot_instance = engine.scene.SpawnPrefab(robot_prefab_id);
        
        engine.tf.SetLocalPosition(robot_instance, pos);
        engine.tf.SetLocalRotation(robot_instance, rot);
        engine.tf.SetLocalScale(robot_instance, scale);
        
        // Create the Render Target FBO and spawn and attach the robot's camera
        cam_target = engine.rhi.CreateRenderTarget(robot_cam_width, robot_cam_height);
        
        rrl::camera::PerspectiveModel camera_model;
        camera_model.fov_y_radians = glm::radians(60.0f);
        camera_model.aspect_ratio = static_cast<float>(robot_cam_width) / static_cast<float>(robot_cam_height);
        camera_model.z_near = 1.0f;     
        camera_model.z_far = 4000.0f;   
        
        robot_cam = engine.camera.SpawnCamera(camera_model, cam_target, rrl::rhi::RHIRenderLayer::LAYER_DEFAULT);
        
        // Attach the camera to the chassis
        engine.tf.AttachChild(robot_instance, robot_cam, rrl::tf::TFDependencyPolicy::CASCADE_DELETE);
        engine.tf.SetLocalPosition(robot_cam, {1.5f, 0.0f, 1.5f});
        engine.tf.SetLocalRotation(robot_cam, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        
        // Spawn visual debugging element
        engine.debug.SpawnCameraFrustum(robot_cam, 3.0f);
        
        // Robot 2D UI POV
        rrl::asset::TextureID fbo_id = "fbo_mirror_" + std::to_string(static_cast<uint32_t>(robot_instance));
        rrl::asset::ImageAsset fbo_image_model;
        fbo_image_model.width           = robot_cam_width;
        fbo_image_model.height          = robot_cam_height;
        fbo_image_model.channels        = rrl::asset::ImageChannelLayout::CH_3;
        fbo_image_model.color_layout    = rrl::asset::ImageColorLayout::RGB;
        fbo_image_model.data_type       = rrl::asset::ImageAssetType::UINT8;
        fbo_image_model.data.resize(robot_cam_width * robot_cam_height * 3, 0);
        
        fbo_tex = engine.asset.CreateTexture(fbo_id, std::move(fbo_image_model));
        
        // Bind the texture to a 2D UI screen layout in the bottom right corner
        ui_screen = engine.scene.SpawnObject();
        engine.asset.BindUITexture(ui_screen, fbo_tex, 0.65f, 0.65f, 0.3f, 0.3f);
    }
    
    void Update(rrl::Engine& engine) {
        if (cam_target != rrl::rhi::TARGET_NULL) {
            // Read back pixels from the GPU FBO and upload them to the UI texture
            rrl::asset::ImageAsset cam_feed = engine.rhi.GetTargetImage(cam_target);
            if (!cam_feed.data.empty()) {
                engine.asset.UpdateTexture(fbo_tex, std::move(cam_feed));
            }
        }
    }

};






int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    LOG_INFO("[RRL Engine] Running Robot on OpenCV Backend");

    uint32_t window_w       = 1280;
    uint32_t window_h       = 720;
    uint32_t robot_cam_w    = 640;
    uint32_t robot_cam_h    = 360;

    // Initialize Engine
    LOG_INFO("[RRL Engine] Booting engine...");
    rrl::Engine engine;

    auto window = engine.rhi.CreateWindow(rrl::rhi::RHIWindowType::GLFW);
    engine.rhi.InitializeWindow(window, "RRL - Robot Simulation SOFRWARE", window_w, window_h);
    engine.rhi.LoadBackend(rrl::rhi::RHIBackendType::SOFTWARE);
    engine.rhi.Initialize(&window);
    engine.debug.SetDebugFlag(rrl::rhi::RHIDebugFlag::FLAG_AFFINE_INTERPOLATION, true);

    engine.debug.SpawnDebugGrid(20.0f, 20);
    engine.debug.SetDebugFlag(rrl::rhi::RHIDebugFlag::FLAG_DRAW_WIREFRAMES, true);


    // Create and spawn the Robot
    LOG_INFO("[RRL Engine] Spawning robot...");
    auto robot = Robot();
    robot.Initialize(engine, robot_cam_w, robot_cam_h);

    // Setup World objects
    LOG_INFO("[RRL Engine] Loading assets...");
    rrl::io::IOPrefab alien_data = rrl::io::LoadPrefab("assets/models/space_kit/Models/OBJ format/alien.obj");
    engine.scene.PreloadPrefabBlueprint("alien", std::move(alien_data));
    rrl::ObjectID alien = engine.scene.SpawnPrefab("alien");
    engine.tf.SetLocalPosition(alien, glm::vec3(5.0f, 0.0f, 0.0f));
    engine.tf.SetLocalScale(alien, glm::vec3(5.0f));

    // Setup Main Camera
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = float(window.width) / float(window.height);
    camera_model.z_near = 1.0f;     
    camera_model.z_far  = 1000.0f;   
    rrl::ObjectID main_camera = engine.camera.SpawnCamera(camera_model);
    engine.camera.SetCameraPositionAndLookAt(main_camera, {-10.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f});
    engine.camera.SetCameraLayer(main_camera, 
        rrl::rhi::RHIRenderLayer::LAYER_DEFAULT | rrl::rhi::RHIRenderLayer::LAYER_DEBUG
    );
    

    // Main Loop
    LOG_INFO("[RRL Engine] Entering main loop...");
    glm::vec3 start_pos = engine.tf.GetLocalPosition(robot.GetInstance());
    float time = 0.0f;
    while (engine.rhi.PollWindowEvents(window)) {
        time += 0.01f;

        // Dynamically rotate the instance
        auto robot_pos = start_pos; 
        robot_pos.x += 1.0f * std::sin(10.0f * time);
        engine.tf.SetLocalPosition(robot.GetInstance(), robot_pos);
        engine.tf.SetLocalRotation(alien, glm::angleAxis(time, glm::vec3(0.0f, 0.0f, 1.0f)));


        // Tick Engine Logic
        engine.tf.UpdateTransformTree();
        engine.camera.UpdateCameras(rrl::camera::NDC_OPENCV); 

        robot.Update(engine);
        engine.rhi.RenderFrame();
    }

    
    flogging::ResetLogger();
    return 0;
}