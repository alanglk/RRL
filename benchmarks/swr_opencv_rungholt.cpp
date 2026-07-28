// benchmarks/swr_opencv_rungholt.cpp

#include <benchmark/benchmark.h>

// Main includes
#include <entt/entt.hpp>
#include <exception>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <FLogging/FLogging.hpp>

// RRL includes
#include <RRL/RRLEngine.hpp>
#include <RRL/RRLTypes.hpp>
#include <RRL/io/PrefabIO.hpp>


class RungholtRenderFixture : public benchmark::Fixture {
public:
    std::unique_ptr<rrl::Engine> engine;
    rrl::rhi::RHIWindow window;
    
    rrl::ObjectID city_instance{rrl::NULL_OBJECT};
    rrl::ObjectID main_camera{rrl::NULL_OBJECT};
    float rotation_z = 0.0f;

    void SetUp(const ::benchmark::State& state) override {
        // Mute logs during performance tracking to keep the terminal clean and avoid console bottlenecks
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Error, flogging::BackendType::StdFormat);

        uint32_t window_w = 1280;
        uint32_t window_h = 720;

        // Initialize Engine
        engine = std::make_unique<rrl::Engine>();
        auto window = engine->rhi.CreateWindow(rrl::rhi::RHIWindowType::OPENCV);
        bool _i = true; // Initialized?
        _i = _i && engine->rhi.InitializeWindow(window, "RRL - Rungholt Benchmark Window", window_w, window_h);
        _i = _i && engine->rhi.LoadBackend(rrl::rhi::RHIBackendType::SOFTWARE);
        _i = _i && engine->rhi.Initialize(&window);
        if (!_i) std::terminate();

        // Camera Setup
        rrl::camera::PerspectiveModel camera_model;
        camera_model.fov_y_radians  = glm::radians(60.0f);
        camera_model.aspect_ratio   = float(window.width) / float(window.height);
        camera_model.z_near         = 0.1f;
        camera_model.z_far          = 100.0f;
        main_camera                 = engine->camera.SpawnCamera(camera_model);
        engine->camera.SetCameraPositionAndLookAt(main_camera, {-6.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f});

        // Load City Assets ONCE during setup phase
        rrl::io::IOPrefab rungholt_data = rrl::io::LoadPrefab("assets/models/rungholt/rungholt.obj");
        engine->scene.PreloadPrefabBlueprint("rungholt_city", std::move(rungholt_data));
        city_instance = engine->scene.SpawnPrefab("rungholt_city");
        engine->tf.SetLocalPosition(city_instance, glm::vec3(0.0f, 0.0f, 0.0f));
        engine->tf.SetLocalScale(city_instance, glm::vec3(0.1f));
    }

    void TearDown(const ::benchmark::State& state) override {
        // Complete memory cleanup when the benchmark run finishes
        engine.reset();
        flogging::ResetLogger();
    }
};


// --- Rendering benchmark --------------------------------------------------
BENCHMARK_DEFINE_F(RungholtRenderFixture, BM_RenderFrames)(benchmark::State& state) {
    
    for (auto _ : state) {
        state.PauseTiming();
        if (city_instance != rrl::NULL_OBJECT) {
            rotation_z += 0.01f;
            glm::quat city_rotation = glm::angleAxis(rotation_z, glm::vec3(0.0f, 0.0f, 1.0f));
            engine->tf.SetLocalRotation(city_instance, city_rotation);
        }
        engine->tf.UpdateTransformTree();
        engine->camera.UpdateCameras(rrl::camera::NDC_OPENCV);
        state.ResumeTiming();


        // Benchmark rendering times ONLY
        engine->rhi.RenderFrame();


        state.PauseTiming();
        engine->rhi.PollWindowEvents(window);
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