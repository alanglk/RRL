// examples/opencv_rungholt.cpp

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <FLogging/FLogging.hpp>



#include "RRL/rhi/RHIAPI.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/scene/SceneManager.hpp"
#include "RRL/io/PrefabIO.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"



int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    entt::registry registry;
    LOG_INFO("Running Rungholt City on OpenCV Backend");

    // Initialize Systems
    rrl::data::InitializeAssetManager(registry);
    rrl::tf::RegisterTFActions(registry);
    rrl::scene::InitializeSceneManager(registry);

    if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::OPENCV, registry)) {
        LOG_ERROR("Failed to load OpenCV RHI backend!");
        return -1;
    }
    rrl::rhi::RHIConfig config{ 1280, 720, "RRL - Rungholt Instancing Viewer", rrl::rhi::RHIRenderingMode::WINDOW };
    rrl::rhi::Initialize(registry, config);

    // Setup Camera
    auto camera_entity = registry.create();
    
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = float(config.width) / float(config.height);
    camera_model.z_near = 1.0f;     
    camera_model.z_far = 4000.0f;   

    // Camera extrinsics
    glm::vec3 cam_pos(-10.0f, 0.0f, 5.0f);
    glm::vec3 target(0.0f, 0.0f, 0.0f);
    glm::vec3 forward = glm::normalize(target - cam_pos); // ISO 8855 rotation (+X Forward, +Y Left, +Z Up)
    glm::vec3 world_up(0.0f, 0.0f, 1.0f);
    glm::vec3 left = glm::normalize(glm::cross(world_up, forward));
    glm::vec3 true_up = glm::cross(forward, left);
    glm::mat3 basis(forward, left, true_up);
    glm::quat cam_rot = glm::quat_cast(basis);

    // Transform and mount Camera
    rrl::tf::AddTransform(registry, camera_entity, cam_pos, cam_rot);
    rrl::camera::AddCamera(registry, camera_entity, camera_model, rrl::rhi::TARGET_MAIN);


    // Load and instantiate the model
    LOG_INFO("Loading city assets...");
    rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/house.obj");
    rrl::scene::PreloadPrefabBlueprint(registry, "rungholt_city", std::move(rungholt_data));
    entt::entity city_instance = rrl::scene::SpawnPrefab(registry, "rungholt_city");
    rrl::tf::SetLocalPosition(registry, city_instance, glm::vec3(0.0f, 0.0f, 0.0f));
    rrl::tf::SetLocalScale(registry, city_instance, glm::vec3(0.1f));


    // Main Loop
    LOG_INFO("Entering main loop...");
    float rotation_z = 0.0f;
    while (true) {
        
        // Dynamically rotate the instance
        if (registry.valid(city_instance)) {
            rotation_z += 0.01f;
            glm::quat city_rotation = glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f));
            rrl::tf::SetLocalRotation(registry, city_instance, city_rotation);
        }

        // Tick Engine Logic
        rrl::tf::UpdateTransformTree(registry);
        rrl::camera::UpdateCameras(registry, rrl::camera::NDC_OPENCV); 

        rrl::rhi::SyncResources(registry);
        rrl::rhi::RenderFrame(registry);
    }

    rrl::rhi::Shutdown(registry);
    flogging::ResetLogger();
    return 0;
}