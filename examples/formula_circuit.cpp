// examples/formula_circuit.cpp

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

#include <FLogging/FLogging.hpp>

// RRL Engine Modules
#include "RRL/data/AssetManager.hpp"
#include "RRL/data/MeshData.hpp"
#include "RRL/data/MaterialData.hpp"
#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"
#include "RRL/rhi/RHIAPI.hpp"

using namespace rrl;

// --- Procedural Geometry Helpers ---

data::MeshData CreateProceduralCone(float radius, float height) {
    data::MeshData mesh;
    mesh.topology = data::MeshTopology::TRIANGLES;
    
    // Top vertex
    mesh.positions.push_back({0.0f, 0.0f, height});
    
    // Base circle
    int segments = 12;
    for (int i = 0; i < segments; ++i) {
        float theta = (2.0f * M_PI * i) / segments;
        mesh.positions.push_back({radius * std::cos(theta), radius * std::sin(theta), 0.0f});
    }

    for (int i = 1; i <= segments; ++i) {
        int next_i = (i % segments) + 1;
        mesh.indices.insert(mesh.indices.end(), {0, static_cast<uint32_t>(i), static_cast<uint32_t>(next_i)});
    }

    mesh.submeshes = {{0, static_cast<uint32_t>(mesh.indices.size())}};
    return mesh;
}

data::MeshData CreateProceduralCar() {
    // A simple stretched box representing the chassis (ISO 8855: +X Forward, +Y Left, +Z Up)
    data::MeshData mesh;
    mesh.topology = data::MeshTopology::TRIANGLES;
    
    float l = 2.0f;  // Half-length (X)
    float w = 0.8f;  // Half-width (Y)
    float h = 0.4f;  // Half-height (Z)
    
    mesh.positions = {
        {-l, -w, -h}, { l, -w, -h}, { l,  w, -h}, {-l,  w, -h}, // Bottom
        {-l, -w,  h}, { l, -w,  h}, { l,  w,  h}, {-l,  w,  h}  // Top
    };
    mesh.indices = {
        0, 1, 2,  0, 2, 3, // Bottom 
        4, 5, 6,  4, 6, 7, // Top 
        0, 1, 5,  0, 5, 4, // Right
        1, 2, 6,  1, 6, 5, // Front
        2, 3, 7,  2, 7, 6, // Left
        3, 0, 4,  3, 4, 7  // Back
    };

    mesh.submeshes = {{0, static_cast<uint32_t>(mesh.indices.size())}};
    return mesh;
}


int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    LOG_INFO("[Formula Example] Booting RRL...");

    // 1. Initialization
    entt::registry registry;
    tf::RegisterTFActions(registry);
    data::InitializeAssetManager(registry);
    
    rhi::RHIWindow main_window = rhi::CreateWindow(rhi::RHIWindowType::GLFW);
    rhi::InitializeWindow(main_window, "RRL - Formula Circuit", 1280, 720);
    
    rhi::LoadBackend(rhi::RHIBackendType::OPENGL, registry);
    rhi::Initialize(registry, &main_window);

    // 2. Materials & Assets
    data::MaterialData mat_cone, mat_car;
    mat_cone.shading_model = data::ShadingModel::UNLIT;
    mat_cone.base_color = glm::vec4(1.0f, 0.4f, 0.0f, 1.0f); // Orange
    
    mat_car.shading_model = data::ShadingModel::UNLIT;
    mat_car.base_color = glm::vec4(0.8f, 0.1f, 0.2f, 1.0f); // Ferrari Red

    entt::entity cone_mat_id = data::CreateMaterial(registry, "mat_cone", mat_cone);
    entt::entity car_mat_id  = data::CreateMaterial(registry, "mat_car", mat_car);

    entt::entity cone_mesh = data::CreateMesh(registry, "mesh_cone", CreateProceduralCone(0.3f, 0.8f));
    entt::entity car_mesh  = data::CreateMesh(registry, "mesh_car", CreateProceduralCar());

    // 3. Track Generation
    float track_radius_x = 40.0f;
    float track_radius_y = 20.0f;
    float track_width    = 6.0f;
    
    int num_cones = 60;
    for (int i = 0; i < num_cones; ++i) {
        float t = (i / static_cast<float>(num_cones)) * 2.0f * M_PI;
        
        // Track centerline
        glm::vec3 pos(track_radius_x * std::cos(t), track_radius_y * std::sin(t), 0.0f);
        
        // Tangent (Forward) and Normal (Left)
        glm::vec3 forward = glm::normalize(glm::vec3(-track_radius_x * std::sin(t), track_radius_y * std::cos(t), 0.0f));
        glm::vec3 left = glm::vec3(-forward.y, forward.x, 0.0f); // 90 deg CCW rotation

        // Spawn Left Cone
        auto cone_l = registry.create();
        tf::AddTransform(registry, cone_l, pos + (left * track_width * 0.5f));
        data::BindMesh(registry, cone_l, cone_mesh, {cone_mat_id});

        // Spawn Right Cone
        auto cone_r = registry.create();
        tf::AddTransform(registry, cone_r, pos - (left * track_width * 0.5f));
        data::BindMesh(registry, cone_r, cone_mesh);
    }

    // 4. Vehicle & Camera Hierarchy Setup
    auto vehicle = registry.create();
    tf::AddTransform(registry, vehicle, glm::vec3(0.0f));
    data::BindMesh(registry, vehicle, car_mesh, {car_mat_id});

    camera::PerspectiveModel cam_model;
    cam_model.fov_y_radians = glm::radians(75.0f);
    cam_model.aspect_ratio = 1280.0f / 720.0f;
    cam_model.z_near = 0.1f;
    cam_model.z_far = 200.0f;

    // Chase Camera (Targets the Screen)
    auto chase_cam = camera::SpawnCamera(registry, cam_model);
    tf::AttachChild(registry, vehicle, chase_cam);
    tf::SetLocalPosition(registry, chase_cam, glm::vec3(-6.0f, 0.0f, 2.5f)); // Behind and up
    tf::SetLocalRotation(registry, chase_cam, glm::quat(glm::vec3(0.0f, glm::radians(15.0f), 0.0f))); // Pitch down 15 deg

    // Sensor Cameras (Targeting offscreen FBOs)
    rhi::RenderTargetHandle fbo_front = rhi::CreateRenderTarget(registry, 640, 480);
    rhi::RenderTargetHandle fbo_left  = rhi::CreateRenderTarget(registry, 640, 480);
    rhi::RenderTargetHandle fbo_right = rhi::CreateRenderTarget(registry, 640, 480);

    auto cam_front = camera::SpawnCamera(registry, cam_model, fbo_front);
    tf::AttachChild(registry, vehicle, cam_front);
    tf::SetLocalPosition(registry, cam_front, glm::vec3(2.0f, 0.0f, 0.5f)); // Front hood

    auto cam_left = camera::SpawnCamera(registry, cam_model, fbo_left);
    tf::AttachChild(registry, vehicle, cam_left);
    tf::SetLocalPosition(registry, cam_left, glm::vec3(0.0f, 1.0f, 0.5f)); // Left side
    tf::SetLocalRotation(registry, cam_left, glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f))); // Yaw +45 deg

    auto cam_right = camera::SpawnCamera(registry, cam_model, fbo_right);
    tf::AttachChild(registry, vehicle, cam_right);
    tf::SetLocalPosition(registry, cam_right, glm::vec3(0.0f, -1.0f, 0.5f)); // Right side
    tf::SetLocalRotation(registry, cam_right, glm::angleAxis(glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f))); // Yaw -45 deg


    // 5. Main Loop
    LOG_INFO("[Formula Example] Starting race...");
    float t = 0.0f;
    float speed = 0.02f;

    while (rhi::PollWindowEvents(main_window)) {
        
        // --- 1. Controller Logic (Drive the Spline) ---
        t += speed;
        if (t >= 2.0f * M_PI) t -= 2.0f * M_PI;

        glm::vec3 pos(track_radius_x * std::cos(t), track_radius_y * std::sin(t), 0.4f);
        
        // Calculate tangent vector to find yaw
        float dx = -track_radius_x * std::sin(t);
        float dy =  track_radius_y * std::cos(t);
        float yaw = std::atan2(dy, dx);

        // Update the Root Entity (The Transform Tree automatically carries the cameras!)
        tf::SetLocalPosition(registry, vehicle, pos);
        tf::SetLocalRotation(registry, vehicle, glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)));

        
        // --- 2. Engine Tick ---
        tf::UpdateTransformTree(registry);
        camera::UpdateCameras(registry, camera::NDC_OPENGL);
        rhi::SyncResources(registry);
        
        rhi::RenderFrame(registry);
        rhi::Present(registry); 
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup
    camera::DestroyAllCameras(registry);
    data::DestroyAllAssets(registry);
    rhi::Shutdown(registry);
    rhi::DestroyWindow(registry, main_window);
    return 0;
}