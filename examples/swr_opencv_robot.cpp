// examples/opencv_robot.cpp

#include <cmath>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <FLogging/FLogging.hpp>


#include "RRL/io/PrefabIO.hpp"

#include "RRL/rhi/RHILayers.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/scene/SceneManager.hpp"
#include "RRL/camera/CameraSystem.hpp"

#include "RRL/rhi/RHIAPI.hpp"
#include "RRL/debug/RHIDebugger.hpp"



static rrl::data::MeshData GenerateCuboidMesh(const glm::vec3& dimensions) {
    rrl::data::MeshData mesh;
    mesh.topology = rrl::data::MeshTopology::TRIANGLES;

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
static rrl::data::ImageData GenerateCheckerboardTexture(uint32_t width, uint32_t height, uint32_t tile_size) {
    rrl::data::ImageData img;
    img.width = width;
    img.height = height;
    img.channels = rrl::data::ImageChannelLayout::CH_3;
    img.color_layout = rrl::data::ImageColorLayout::RGB;
    img.data_type = rrl::data::ImageDataType::UINT8;
    
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
static rrl::data::MeshData GenerateFrustumScreenMesh() {
    rrl::data::MeshData quad;
    quad.topology = rrl::data::MeshTopology::TRIANGLES;
    quad.positions = {{-0.8f, -0.45f, 0.0f}, { 0.8f, -0.45f, 0.0f}, { 0.8f,  0.45f, 0.0f}, {-0.8f,  0.45f, 0.0f}};
    quad.uvs = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    quad.indices = { 0, 1, 2, 2, 3, 0 };
    return quad;
}







class Robot {
private:
    static constexpr char robot_prefab_id[6] = "robot";
    static void PreloadRobotPrefab(entt::registry& registry) {
        // Build the Prefab structure
        rrl::io::IOPrefab robot_prefab; 
        robot_prefab.filepath = robot_prefab_id;

        // Load the robot's texture
        auto checkerboard = GenerateCheckerboardTexture(512, 512, 64);
        rrl::data::TextureID virtual_tex_path = "robot_albedo";
        rrl::data::CreateTexture(registry, virtual_tex_path, std::move(checkerboard));

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
        rrl::scene::PreloadPrefabBlueprint(registry, robot_prefab_id, std::move(robot_prefab));
    }

    // Robot private attribs
    entt::entity robot_instance = entt::null;
    entt::entity robot_cam      = entt::null;
    entt::entity ui_screen      = entt::null;
    entt::entity fbo_tex        = entt::null;
    rrl::rhi::RenderTargetHandle cam_target = rrl::rhi::TARGET_NULL;
    
public:
    Robot() = default;
    entt::entity GetInstance() const { return robot_instance; }

    void Initialize(entt::registry& registry, 
        uint32_t robot_cam_width, uint32_t robot_cam_height,
        glm::vec3 pos = {0.0f, 0.0f, 0.0f}, 
        glm::quat rot = {1.0f, 0.0f, 0.0f, 0.0f},
        glm::vec3 scale = {1.0f, 1.0f, 1.0f}
    ) {
    
        // Preload and Spawn Robot
        PreloadRobotPrefab(registry);
        robot_instance = rrl::scene::SpawnPrefab(registry, robot_prefab_id);
        
        rrl::tf::SetLocalPosition(registry, robot_instance, pos);
        rrl::tf::SetLocalRotation(registry, robot_instance, rot);
        rrl::tf::SetLocalScale(registry, robot_instance, scale);
        
        // Create the Render Target FBO and spawn and attach the robot's camera
        cam_target = rrl::rhi::CreateRenderTarget(registry, robot_cam_width, robot_cam_height);
        rrl::camera::PerspectiveModel camera_model;
        camera_model.fov_y_radians = glm::radians(60.0f);
        camera_model.aspect_ratio = static_cast<float>(robot_cam_width) / static_cast<float>(robot_cam_height);
        camera_model.z_near = 1.0f;     
        camera_model.z_far = 4000.0f;   
        robot_cam = rrl::camera::SpawnCamera(registry, camera_model, cam_target, rrl::rhi::RenderLayer::LAYER_DEFAULT);
        rrl::tf::AttachChild(registry, robot_instance, robot_cam, rrl::tf::TFDependencyPolicy::CASCADE_DELETE);
        rrl::tf::SetLocalPosition(registry, robot_cam, {1.5f, 0.0f, 1.5f});
        rrl::tf::SetLocalRotation(registry, robot_cam, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        rrl::debug::rhi::SpawnCameraFrustum(registry, robot_cam, 3.0f);
        
        
        // Robot 2D UI POV
        rrl::data::TextureID fbo_id = "fbo_mirror_" + std::to_string(static_cast<uint32_t>(robot_instance));
        rrl::data::ImageData fbo_image_model;
        fbo_image_model.width = robot_cam_width;
        fbo_image_model.height = robot_cam_height;
        fbo_image_model.channels = rrl::data::ImageChannelLayout::CH_3;
        fbo_image_model.color_layout = rrl::data::ImageColorLayout::RGB;
        fbo_image_model.data_type = rrl::data::ImageDataType::UINT8;
        fbo_image_model.data.resize(robot_cam_width * robot_cam_height * 3, 0);
        fbo_tex = rrl::data::CreateTexture(registry, fbo_id, std::move(fbo_image_model));
        
        // Bind the texture to a 2D UI screen layout in the bottom right corner
        ui_screen = registry.create();
        rrl::data::BindUITexture(registry, ui_screen, fbo_tex, 0.65f, 0.65f, 0.3f, 0.3f);
    }
    
    void Update(entt::registry& registry) {
        if (cam_target != rrl::rhi::TARGET_NULL) {
            rrl::data::ImageData cam_feed = rrl::rhi::GetTargetImage(registry, cam_target);
            if (!cam_feed.data.empty()) {
                rrl::data::UpdateTexture(registry, fbo_tex, std::move(cam_feed));
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

    // Initialize Systems
    LOG_INFO("[RRL Engine] Booting subsystems...");
    entt::registry registry;
    rrl::data::InitializeAssetManager(registry);
    rrl::tf::RegisterTFActions(registry);
    rrl::scene::InitializeSceneManager(registry);
    rrl::rhi::RHIWindow main_window = rrl::rhi::CreateWindow(rrl::rhi::RHIWindowType::OPENCV);
    rrl::rhi::InitializeWindow(main_window, "RRL - Robot Simulation", window_w, window_h);
    if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::SOFTWARE, registry)) {
        LOG_ERROR("[RRL Engine] Failed to load RHI backend!");
        return -1;
    }
    if (!rrl::rhi::Initialize(registry, &main_window)) {
        LOG_ERROR("[RRL Engine] Failed to initialize RHI backend!");
        return -1;
    }

    rrl::debug::rhi::SpawnDebugGrid(registry, 20.0f, 20);
    // rrl::debug::rhi::SetDebugFlag(registry, rrl::rhi::RHIDebugFlag::FLAG_DRAW_WIREFRAMES, true);


    // Create and spawn the Robot
    LOG_INFO("[RRL Engine] Spawning robot...");
    auto robot = Robot();
    robot.Initialize(registry, robot_cam_w, robot_cam_h);

    // Setup World objects
    LOG_INFO("[RRL Engine] Loading assets...");
    rrl::io::IOPrefab alien_data = rrl::io::LoadPrefab("assets/models/space_kit/Models/OBJ format/alien.obj");
    rrl::scene::PreloadPrefabBlueprint(registry, "alien", std::move(alien_data));
    entt::entity alien = rrl::scene::SpawnPrefab(registry, "alien");
    rrl::tf::SetLocalPosition(registry, alien, glm::vec3(5.0f, 0.0f, 0.0f));
    rrl::tf::SetLocalScale(registry, alien, glm::vec3(5.0f));

    // Setup Main Camera
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = float(main_window.width) / float(main_window.height);
    camera_model.z_near = 1.0f;     
    camera_model.z_far = 1000.0f;   
    entt::entity main_camera = rrl::camera::SpawnCamera(registry, camera_model);
    rrl::camera::SetCameraPositionAndLookAt(registry, main_camera, {-10.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f});
    rrl::camera::SetCameraLayer(registry, main_camera, 
        rrl::rhi::RenderLayer::LAYER_DEFAULT | rrl::rhi::RenderLayer::LAYER_DEBUG
    );
    

    // Main Loop
    LOG_INFO("[RRL Engine] Entering main loop...");
    glm::vec3 start_pos = rrl::tf::GetLocalPosition(registry, robot.GetInstance());
    float time = 0.0f;
    while (true) {
        time += 0.01f;

        // Dynamically rotate the instance
        auto robot_pos = start_pos; 
        robot_pos.x += 1.0f * std::sin(10.0f * time);
        rrl::tf::SetLocalPosition(registry, robot.GetInstance(), robot_pos);
        rrl::tf::SetLocalRotation(registry, alien, glm::angleAxis(time, glm::vec3(0.0f, 0.0f, 1.0f)));


        // Tick Engine Logic
        rrl::tf::UpdateTransformTree(registry);
        rrl::camera::UpdateCameras(registry, rrl::camera::NDC_OPENCV); 

        robot.Update(registry);
        rrl::rhi::SyncResources(registry);
        rrl::rhi::RenderFrame(registry);
        rrl::rhi::Present(registry);
        rrl::rhi::PollWindowEvents(main_window); 
    }

    
    flogging::ResetLogger();
    return 0;
}