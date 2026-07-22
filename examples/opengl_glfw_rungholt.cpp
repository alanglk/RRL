// examples/opengl_glfw_rungholt.cpp

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <FLogging/FLogging.hpp>


// RRL Engine Modules
#include <RRL/RRLEngine.hpp>
#include <RRL/io/PrefabIO.hpp>


int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    LOG_INFO("[RRL Engine] Running Rungholt City on OpenCV Backend");

    uint32_t window_w       = 1280;
    uint32_t window_h       = 720;

    // Initialize Engine
    LOG_INFO("[RRL Engine] Booting engine...");
    rrl::Engine engine;

    auto window = engine.rhi.CreateWindow(rrl::rhi::RHIWindowType::GLFW);
    engine.rhi.InitializeWindow(window, "RRL - Rungholt Instancing Viewer", window_w, window_h);
    engine.rhi.LoadBackend(rrl::rhi::RHIBackendType::OPENGL);
    engine.rhi.Initialize(&window);
    engine.debug.SetDebugFlag(rrl::rhi::RHIDebugFlag::FLAG_AFFINE_INTERPOLATION, true);


    // Camera Setup
    rrl::camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians  = glm::radians(60.0f);
    camera_model.aspect_ratio   = float(window.width) / float(window.height);
    camera_model.z_near = 1.0f;     
    camera_model.z_far = 4000.0f;   
    rrl::ObjectID main_camera   = engine.camera.SpawnCamera(camera_model);
    engine.camera.SetCameraPositionAndLookAt(main_camera, {-8.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f});

    // Load and instantiate the model
    LOG_INFO("[RRL Engine] Loading city assets...");
    rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/rungholt.obj");
    engine.asset.PreloadPrefabBlueprint("rungholt_city", std::move(rungholt_data));
    rrl::ObjectID city_instance = engine.scene.SpawnPrefab("rungholt_city");
    engine.tf.SetLocalPosition(city_instance, glm::vec3(0.0f, 0.0f, 0.0f));
    engine.tf.SetLocalScale(city_instance, glm::vec3(0.1f));

    // Prefabs instancing has the ability to instance specific objects.
    // Try calling engine.scene.SpawnPrefab() with:
    //  - "rungholt_city"
    //  - "rungholt_city.cobblestone"
    //  - "rungholt_city.stone"
    //
    // TODO: materials are getting pruned when instantiating prefab sub nodes!!
    //

    // Main Loop
    LOG_INFO("[RRL Engine] Entering main loop...");
    float rotation_z = 0.0f;
    while (engine.rhi.PollWindowEvents(window)) {
        
        // Dynamically rotate the instance
        rotation_z += 0.01f;
        glm::quat city_rotation = glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f));
        engine.tf.SetLocalRotation(city_instance, city_rotation);

        // Tick Engine Logic
        engine.tf.UpdateTransformTree();
        engine.camera.UpdateCameras(rrl::camera::NDC_OPENGL); 
        engine.rhi.RenderFrame();
    }


    // Cleanup (engine handles its shutdown on its destructor)
    LOG_INFO("[RRL Engine] Shutting down...");
    flogging::ResetLogger();
    return 0;
}