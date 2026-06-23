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
    flogging::InitLogger(flogging::LogLevel::Info, flogging::BackendType::StdFormat);
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
    // Setup Camera
    auto camera_entity = registry.create();
    
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = float(config.width) / float(config.height);
    camera_model.z_near = 1.0f;     
    camera_model.z_far = 3000.0f;   
    glm::vec3 cam_pos(-150.0f, 0.0f, 100.0f); 

    // ISO 8855 Look-At rotation (+X Forward, +Y Left, +Z Up)
    glm::vec3 target(0.0f, 0.0f, 0.0f);
    glm::vec3 forward = glm::normalize(target - cam_pos);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    
    // Right-handed cross products to build the orthonormal basis
    glm::vec3 left = glm::normalize(glm::cross(up, forward));
    glm::vec3 true_up = glm::cross(forward, left);
    glm::mat3 basis(forward, left, true_up);
    glm::quat cam_rot = glm::quat_cast(basis);

    // Transform and mount Camera
    rrl::tf::AddTransform(registry, camera_entity, cam_pos, cam_rot);
    rrl::camera::AddCamera(registry, camera_entity, camera_model, rrl::rhi::TARGET_MAIN);


    // Load and instantiate the model
    LOG_INFO("Loading city assets...");
    rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/rungholt.obj");
    rrl::scene::PreloadPrefabBlueprint(registry, "RungholtCity", std::move(rungholt_data));
    entt::entity city_instance = rrl::scene::SpawnPrefab(registry, "RungholtCity");
    rrl::tf::SetLocalScale(registry, city_instance, glm::vec3(0.1f));


    // Main Loop
    LOG_INFO("Entering main loop...");
    float rotation_y = 0.0f;
    while (true) {
        
        // Dynamically rotate the instance
        if (registry.valid(city_instance)) {
            rotation_y += 0.01f;
            rrl::tf::SetLocalRotation(registry, city_instance, glm::quat(glm::vec3(0.0f, rotation_y, 0.0f)));
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