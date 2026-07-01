// examples/opencv_rungholt.cpp

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <FLogging/FLogging.hpp>



#include "RRL/rhi/RHIAPI.hpp"
#include "RRL/debug/RHIDebugger.hpp"

#include "RRL/data/AssetManager.hpp"
#include "RRL/scene/SceneManager.hpp"
#include "RRL/io/PrefabIO.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"



int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    LOG_INFO("[RRL Engine] Running Rungholt City on OpenCV Backend");

    uint32_t window_w       = 1280;
    uint32_t window_h       = 720;

    // Initialize Systems
    LOG_INFO("[RRL Engine] Booting subsystems...");
    entt::registry registry;
    rrl::data::InitializeAssetManager(registry);
    rrl::tf::RegisterTFActions(registry);
    rrl::scene::InitializeSceneManager(registry);
    rrl::rhi::RHIWindow main_window = rrl::rhi::CreateWindow(rrl::rhi::RHIWindowType::OPENCV);
    rrl::rhi::InitializeWindow(main_window, "RRL - Rungholt Instancing Viewer", window_w, window_h);
    if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::SOFTWARE, registry)) {
        LOG_ERROR("[RRL Engine] Failed to load RHI backend!");
        return -1;
    }
    if (!rrl::rhi::Initialize(registry, &main_window)) {
        LOG_ERROR("[RRL Engine] Failed to initialize RHI backend!");
        return -1;
    }
    // rrl::debug::rhi::SetDebugFlag(registry, rrl::rhi::RHIDebugFlag::FLAG_AFFINE_INTERPOLATION, true);


    // Setup Camera
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = float(main_window.width) / float(main_window.height);
    camera_model.z_near = 1.0f;     
    camera_model.z_far = 4000.0f;   
    entt::entity main_camera = rrl::camera::SpawnCamera(registry, camera_model);
    rrl::camera::SetCameraPositionAndLookAt(registry, main_camera, {-8.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f});

    // Load and instantiate the model
    LOG_INFO("[RRL Engine] Loading city assets...");
    rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/house.obj");
    rrl::scene::PreloadPrefabBlueprint(registry, "rungholt_city", std::move(rungholt_data));
    entt::entity city_instance = rrl::scene::SpawnPrefab(registry, "rungholt_city");
    rrl::tf::SetLocalPosition(registry, city_instance, glm::vec3(0.0f, 0.0f, 0.0f));
    rrl::tf::SetLocalScale(registry, city_instance, glm::vec3(0.1f));


    // Main Loop
    LOG_INFO("[RRL Engine] Entering main loop...");
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
        rrl::rhi::Present(registry);
        rrl::rhi::PollWindowEvents(main_window); 
    }


    // Cleanup
    rrl::camera::DestroyAllCameras(registry);
    rrl::data::DestroyAllAssets(registry);
    rrl::rhi::Shutdown(registry);
    flogging::ResetLogger();
    return 0;
}