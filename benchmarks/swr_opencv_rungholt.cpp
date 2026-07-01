// benchmarks/swr_opencv_rungholt.cpp

#include <benchmark/benchmark.h>

// Main includes
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <FLogging/FLogging.hpp>

// RRL includes
#include "RRL/rhi/RHIAPI.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/scene/SceneManager.hpp"
#include "RRL/io/PrefabIO.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"



class RungholtRenderFixture : public benchmark::Fixture {
public:
    entt::registry registry;
    rrl::rhi::RHIWindow main_window;
    entt::entity city_instance{entt::null};
    entt::entity main_camera{entt::null};
    float rotation_z = 0.0f;

    void SetUp(const ::benchmark::State& state) override {
        // Mute logs during performance tracking to keep the terminal clean and avoid console bottlenecks
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Error, flogging::BackendType::StdFormat);

        uint32_t window_w = 1280;
        uint32_t window_h = 720;

        // Initialize Systems
        rrl::data::InitializeAssetManager(registry);
        rrl::tf::RegisterTFActions(registry);
        rrl::scene::InitializeSceneManager(registry);
        
        main_window = rrl::rhi::CreateWindow(rrl::rhi::RHIWindowType::OPENCV);
        rrl::rhi::InitializeWindow(main_window, "RRL - Rungholt Benchmark Window", window_w, window_h);
        if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::SOFTWARE, registry) ||
            !rrl::rhi::Initialize(registry, &main_window)) {
            std::terminate();
        }

        // Setup Camera
        rrl::camera::PerspectiveModel camera_model;
        camera_model.fov_y_radians = glm::radians(60.0f);
        camera_model.aspect_ratio = float(main_window.width) / float(main_window.height);
        camera_model.z_near = 1.0f;     
        camera_model.z_far = 4000.0f;   
        main_camera = rrl::camera::SpawnCamera(registry, camera_model);
        rrl::camera::SetCameraPositionAndLookAt(registry, main_camera, {-8.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f});

        // Load City Assets ONCE during setup phase
        rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/rungholt.obj");
        rrl::scene::PreloadPrefabBlueprint(registry, "rungholt_city", std::move(rungholt_data));
        city_instance = rrl::scene::SpawnPrefab(registry, "rungholt_city");
        rrl::tf::SetLocalPosition(registry, city_instance, glm::vec3(0.0f, 0.0f, 0.0f));
        rrl::tf::SetLocalScale(registry, city_instance, glm::vec3(0.1f));
    }

    void TearDown(const ::benchmark::State& state) override {
        // Complete memory cleanup when the benchmark run finishes
        rrl::camera::DestroyAllCameras(registry);
        rrl::data::DestroyAllAssets(registry);
        rrl::rhi::Shutdown(registry);
        flogging::ResetLogger();
    }
};


// --- Rendering benchmark --------------------------------------------------
BENCHMARK_DEFINE_F(RungholtRenderFixture, BM_RenderFrames)(benchmark::State& state) {
    
    for (auto _ : state) {
        state.PauseTiming();
        if (registry.valid(city_instance)) {
            rotation_z += 0.01f;
            glm::quat city_rotation = glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f));
            rrl::tf::SetLocalRotation(registry, city_instance, city_rotation);
        }
        rrl::tf::UpdateTransformTree(registry);
        rrl::camera::UpdateCameras(registry, rrl::camera::NDC_OPENCV); 
        rrl::rhi::SyncResources(registry);
        state.ResumeTiming();


        // Benchmark rendering times ONLY
        rrl::rhi::RenderFrame(registry);


        state.PauseTiming();
        rrl::rhi::Present(registry);
        rrl::rhi::PollWindowEvents(main_window); 
        state.ResumeTiming(); 
    }

    // Calculate and print an FPS metric in your results
    state.counters["FPS"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}




// --- Registration ---------------------------------------------------------
BENCHMARK_REGISTER_F(RungholtRenderFixture, BM_RenderFrames)
    ->Iterations(1000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_MAIN();