



#include "RRL/data/ImageData.hpp"
#include "RRL/io/PrefabIO.hpp"
#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/scene/SceneManager.hpp"
#include "RRL/camera/CameraSystem.hpp"
#include "RRL/rhi/RHIAPI.hpp"
#include "entt/entity/fwd.hpp"



#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <FLogging/FLogging.hpp>





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
        std::string virtual_tex_path = "robot_albedo";
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

        // Pack the materials into the prefab
        entt::id_type mat_hash = entt::hashed_string("robot_chassis_mat").value();
        chassis_node.mesh.materials.push_back({
            0, // index offset
            static_cast<uint32_t>(chassis_node.mesh.indices.size()), // index count
            static_cast<entt::entity>(mat_hash)                  // Pack the string hash
        });
        robot_prefab.root_nodes.push_back(std::move(chassis_node));
        rrl::scene::PreloadPrefabBlueprint(registry, robot_prefab_id, std::move(robot_prefab));
    }

    // Robot private attribs
    entt::entity robot_instance = entt::null;
    entt::entity robot_cam      = entt::null;
    entt::entity frustum_screen = entt::null;
    entt::entity fbo_tex        = entt::null;
    rrl::rhi::RenderTargetHandle cam_target = rrl::rhi::TARGET_NULL;
    
public:
    Robot() { 
    }

    void Initialize(entt::registry& registry, 
            uint32_t robot_cam_width, uint32_t robot_cam_height,
            glm::vec3 pos = {0.0f, 0.0f, 0.0f}, 
            glm::quat rot = {1.0f, 0.0f, 0.0f, 0.0f},
            glm::vec3 scale = {1.0f, 1.0f, 1.0f}
        ) {
        

        // Preload and Spawn
        PreloadRobotPrefab(registry);
        robot_instance = rrl::scene::SpawnPrefab(registry, robot_prefab_id);
        
        rrl::tf::SetLocalPosition(registry, robot_instance, pos);
        rrl::tf::SetLocalRotation(registry, robot_instance, rot);
        rrl::tf::SetLocalScale(registry, robot_instance, scale);
        
        // Spawn robot's cameras
        cam_target = rrl::rhi::CreateRenderTarget(registry, robot_cam_width, robot_cam_height);
        robot_cam = registry.create(); 
        
        rrl::camera::PerspectiveModel camera_model;
        camera_model.fov_y_radians = glm::radians(60.0f);
        camera_model.aspect_ratio = static_cast<float>(robot_cam_width) / static_cast<float>(robot_cam_height);
        camera_model.z_near = 1.0f;     
        camera_model.z_far = 4000.0f;   

        glm::vec3 cam_pos(1.5f, 0.0f, 1.5f); // Placed slightly forward (+X) and up (+Z) on the mast
        glm::vec3 target(5.0f, 0.0f, 1.5f);  // Looking straight ahead
        glm::vec3 forward = glm::normalize(target - cam_pos); 
        glm::vec3 world_up(0.0f, 0.0f, 1.0f); // ISO 8855 Up
        glm::vec3 left = glm::normalize(glm::cross(world_up, forward));
        glm::vec3 true_up = glm::cross(forward, left);
        glm::mat3 basis(forward, left, true_up);
        glm::quat cam_rot = glm::quat_cast(basis);

        rrl::tf::AddTransform(registry, robot_cam, robot_instance, cam_pos, cam_rot, glm::vec3(1.0f));
        rrl::camera::AddCamera(registry, robot_cam, camera_model, cam_target);
        
        // Frustum Screen
        std::string fbo_id = "fbo_mirror_" + std::to_string(static_cast<uint32_t>(robot_instance));
        rrl::data::ImageData fbo_image_model;
        fbo_image_model.width = robot_cam_width;
        fbo_image_model.height = robot_cam_height;
        fbo_image_model.channels = rrl::data::ImageChannelLayout::CH_3;
        fbo_image_model.color_layout = rrl::data::ImageColorLayout::RGB;
        fbo_image_model.data_type = rrl::data::ImageDataType::UINT8;
        fbo_image_model.data.resize(robot_cam_width * robot_cam_height * 3, 0);
        fbo_tex = rrl::data::CreateTexture(registry, fbo_id, std::move(fbo_image_model));
        
        rrl::data::MaterialData screen_mat_data;
        entt::entity screen_mat = rrl::data::CreateMaterial(registry, fbo_id + "_mat", screen_mat_data);
        rrl::data::BindMaterialTexture(registry, screen_mat, fbo_tex, rrl::data::MaterialTextureType::ALBEDO);

        entt::entity screen_mesh = rrl::data::CreateMesh(registry, "frustum_quad", GenerateFrustumScreenMesh());
        rrl::data::BindMaterial(registry, screen_mesh, screen_mat);
        
        frustum_screen = registry.create();
        rrl::data::BindMesh(registry, frustum_screen, screen_mesh);
        glm::quat screen_rot = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        rrl::tf::AddTransform(registry, frustum_screen, robot_cam, glm::vec3(2.0f, 0.0f, 0.0f), screen_rot);
    }
    
    void Update(entt::registry& registry) {
        if (cam_target != rrl::rhi::TARGET_NULL) {
            rrl::data::ImageData cam_feed = rrl::rhi::GetTargetImage(registry, cam_target);
            if (!cam_feed.data.empty()) {
                rrl::data::UpdateTexture(registry, fbo_tex, std::move(cam_feed));
            }
        }
    }

    entt::entity GetInstance() const { return robot_instance; }
};




int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    entt::registry registry;
    LOG_INFO("Running Robot on OpenCV Backend");

    uint32_t window_w       = 1280;
    uint32_t window_h       = 720;
    uint32_t robot_cam_w    = 640;
    uint32_t robot_cam_h    = 360;

    // Initialize Systems
    rrl::data::InitializeAssetManager(registry);
    rrl::tf::RegisterTFActions(registry);
    rrl::scene::InitializeSceneManager(registry);

    if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::OPENCV, registry)) {
        LOG_ERROR("Failed to load OpenCV RHI backend!");
        return -1;
    }
    rrl::rhi::RHIConfig config{ window_w, window_h, "RRL - Rungholt Instancing Viewer", rrl::rhi::RHIRenderingMode::WINDOW };
    rrl::rhi::Initialize(registry, config);



    // Create and spawn the Robot
    LOG_INFO("Spawning robot...");
    auto robot = Robot();
    robot.Initialize(registry, robot_cam_w, robot_cam_h);

    // Setup World objects
    LOG_INFO("Loading city assets...");
    rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/house.obj");
    rrl::scene::PreloadPrefabBlueprint(registry, "rungholt_city", std::move(rungholt_data));
    entt::entity city_instance = rrl::scene::SpawnPrefab(registry, "rungholt_city");
    rrl::tf::SetLocalPosition(registry, city_instance, glm::vec3(5.0f, 0.0f, 0.0f));
    rrl::tf::SetLocalScale(registry, city_instance, glm::vec3(0.1f));


    // Setup Main Camera
    LOG_INFO("Creating main camera");
    entt::entity main_cam = registry.create();
    rrl::camera::PerspectiveModel main_cam_model;
    main_cam_model.fov_y_radians = glm::radians(75.0f);
    main_cam_model.aspect_ratio = static_cast<float>(config.width) / static_cast<float>(config.height);
    main_cam_model.z_near = 0.1f;     
    main_cam_model.z_far = 1000.0f;  

    glm::vec3 mcam_pos(-10.0f, 0.0f, 8.0f);
    glm::vec3 mcam_target(0.0f, 0.0f, 0.0f); // Lock onto the origin!
    glm::vec3 mcam_forward = glm::normalize(mcam_target - mcam_pos);
    glm::vec3 mcam_up(0.0f, 0.0f, 1.0f);
    glm::vec3 mcam_left = glm::normalize(glm::cross(mcam_up, mcam_forward));
    glm::vec3 mcam_true_up = glm::cross(mcam_forward, mcam_left);
    glm::quat mcam_rot = glm::quat_cast(glm::mat3(mcam_forward, mcam_left, mcam_true_up));

    rrl::tf::AddTransform(registry, main_cam, mcam_pos, mcam_rot);
    rrl::camera::AddCamera(registry, main_cam, main_cam_model);


    // Main Loop
    LOG_INFO("Entering main loop...");
    float rotation_z = 0.0f;
    while (true) {
        // Dynamically rotate the instance
        rotation_z += 0.02f;
        rrl::tf::SetLocalRotation(registry, robot.GetInstance(), glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f)));


        // Tick Engine Logic
        rrl::tf::UpdateTransformTree(registry);
        rrl::camera::UpdateCameras(registry, rrl::camera::NDC_OPENCV); 

        robot.Update(registry);
        rrl::rhi::SyncResources(registry);
        rrl::rhi::RenderFrame(registry);
    }

    
    flogging::ResetLogger();
    return 0;
}