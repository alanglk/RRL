
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <chrono>
#include <thread>

#include <FLogging/FLogging.hpp>

// RRL Engine Modules
#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"
#include "RRL/data/MeshData.hpp"
#include "RRL/rhi/RHIAPI.hpp"


using namespace rrl;



// Helper Function to Create a 3D Cube
data::MeshData CreateWireframeCube(float size) {
    data::MeshData mesh;
    mesh.topology = data::MeshTopology::TRIANGLES;
    float h = size * 0.5f;

    // 8 Corner Vertices of a Cube (ISO 8855 Coordinate System)
    mesh.positions = {
        {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h}, // Bottom 4 corners
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}  // Top 4 corners
    };
    mesh.indices = {
        0, 1, 2,  0, 2, 3, // Bottom face
        4, 5, 6,  4, 6, 7, // Top face
        0, 1, 5,  0, 5, 4, // Front face
        1, 2, 6,  1, 6, 5, // Right face
        2, 3, 7,  2, 7, 6, // Back face
        3, 0, 4,  3, 4, 7  // Left face
    };

    return mesh;
}




int main() {
    flogging::InitLogger(flogging::LogLevel::Warn, flogging::BackendType::StdFormat);
    entt::registry registry;

    LOG_INFO("[RRL Engine] Booting subsystems...");
    tf::RegisterTFActions(registry);
    rhi::LoadBackend(rhi::RHIBackendType::OPENCV, registry);


    rhi::RHIConfig config;
    config.width = 1024;
    config.height = 768;
    config.title = "RRL - OpenCV Rasterizer";
    config.mode = rhi::RHIRenderingMode::WINDOW;

    if (!rhi::Initialize(registry, config)) {
        LOG_ERROR("[RRL Engine] Failed to initialize RHI backend!");
        return -1;
    }

    // Setup Scene Objects
    auto cube_entity = registry.create();
    tf::AddTransform(registry, cube_entity, glm::vec3(0.0f, 0.0f, 0.0f));
    registry.emplace<data::MeshData>(cube_entity, CreateWireframeCube(2.0f)); // 2-meter cube

    // Create a root gimbal mount entity located 6 meters back and 3 meters up
    auto rig_entity = registry.create();
    tf::AddTransform(registry, rig_entity, glm::vec3(-6.0f, 0.0f, 3.0f));

    // Create the actual Camera Entity, attached as a child to the rig
    auto camera_entity = registry.create();
    tf::AddTransform(registry, camera_entity, rig_entity, glm::vec3(0.0f, 0.0f, 0.0f));

    // Configure and mount the Camera Component as the Primary Screen Output
    camera::PerspectiveModel camera_model;
    camera_model.fov_y_radians = glm::radians(60.0f);
    camera_model.aspect_ratio = 1024.0f / 768.0f;
    camera_model.z_near = 0.1f;
    camera_model.z_far = 100.0f;

    camera::AddCamera(registry, camera_entity, camera_model, rhi::TARGET_MAIN);


    // Main loop
    LOG_INFO("[RRL Engine] Entering main loop...");
    float time = 0.0f;
    while (true) {

        // Simple orbital logic: Move the rig in a circle around the origin over time
        time += 0.015f;
        float radius = 7.0f;
        glm::vec3 dynamic_position(cos(time) * radius, sin(time) * radius, 3.0f);
        
        // Calculate orientation pointing toward the origin (ISO 8855: +X is Forward, +Y is Left, +Z is Up)
        glm::vec3 forward = glm::normalize(glm::vec3(0.0f) - dynamic_position);
        glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
        
        // In a Right-Handed system: Up x Forward = Left
        glm::vec3 left = glm::normalize(glm::cross(up, forward)); 
        glm::vec3 true_up = glm::cross(forward, left);
        glm::mat3 basis(forward, left, true_up);
        glm::quat dynamic_rotation = glm::quat_cast(basis);


        // Safely update the local transform components inside the ECS
        tf::SetLocalPosition(registry, rig_entity, dynamic_position);
        tf::SetLocalRotation(registry, rig_entity, dynamic_rotation);


        // Tick Update Sequence 
        tf::UpdateTransformTree(registry);
        camera::UpdateCameras(registry, camera::NDC_OPENCV);
        rhi::RenderFrame(registry);


        // ~60 FPS main loop
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }


    // Cleanup
    rhi::Shutdown(registry);
    flogging::ResetLogger();
    return 0;
}