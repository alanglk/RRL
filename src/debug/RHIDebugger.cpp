// RRL/src/debug/RHIDebugger.cpp

#include "RRL/debug/RHIDebugger.hpp"

#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/rhi/RHILayers.hpp"
#include "RRL/rhi/RHIBackendManager.hpp"

#include "RRL/tf/TransformTree.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/camera/CameraSystem.hpp"



namespace rrl::debug::rhi {
    

// --- Helpers -----------------------------------------------------
static entt::entity GetOrCreateWireframeDebugMaterial(entt::registry& registry) {
    std::string mat_id = "debug_wireframe_mat";
    entt::entity mat = data::GetCachedMaterial(registry, mat_id);
    if (mat != entt::null) return mat;
    
    data::MaterialData debug_mat;
    debug_mat.base_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    return data::CreateMaterial(registry, mat_id, debug_mat);
}
static data::MeshData CreatePerspectiveCameraMesh(const camera::PerspectiveModel& model, float visual_length) {
    float h_near    = model.z_near * std::tan(model.fov_y_radians * 0.5f);
    float w_near    = h_near * model.aspect_ratio;
    float h_far     = visual_length * std::tan(model.fov_y_radians * 0.5f);
    float w_far     = h_far * model.aspect_ratio;

    // ISO 8855 Vertices (+X Forward, +Y Left, +Z Up)
    data::MeshData frustum_mesh;
    frustum_mesh.topology = data::MeshTopology::LINES;
    frustum_mesh.positions = {
        // Near Plane (X = z_near)
        {model.z_near,  w_near,  h_near}, // 0: Top-Left
        {model.z_near,  w_near, -h_near}, // 1: Bottom-Left
        {model.z_near, -w_near, -h_near}, // 2: Bottom-Right
        {model.z_near, -w_near,  h_near}, // 3: Top-Right

        // Far Plane (X = visual_length)
        {visual_length,  w_far,  h_far}, // 4: Top-Left
        {visual_length,  w_far, -h_far}, // 5: Bottom-Left
        {visual_length, -w_far, -h_far}, // 6: Bottom-Right
        {visual_length, -w_far,  h_far}  // 7: Top-Right
    };

    // Define the wireframe line indices
    frustum_mesh.indices = {
        // Near rect
        0, 1,  1, 2,  2, 3,  3, 0,
        // Far rect
        4, 5,  5, 6,  6, 7,  7, 4,
        // Connecting lines
        0, 4,  1, 5,  2, 6,  3, 7
    };

    return frustum_mesh;
}



// --- API ---------------------------------------------------------
RHIDebugReport GetRHIDebugReport(entt::registry& registry) {
    RHIDebugReport report;
    auto& backend = rrl::rhi::RHIBackendManager::Instance().GetBackend();
    if (backend.GetActiveDebugFlags) {
        report.active_flag = backend.GetActiveDebugFlags(registry);
    }
    return report;
}
void SetDebugFlag(entt::registry& registry, rrl::rhi::RHIDebugFlag flag, bool enable) {
    if (registry.ctx().contains<rrl::rhi::RHIBackend>()) {
        auto& backend = registry.ctx().get<rrl::rhi::RHIBackend>();
        if (backend.SetDebugFlag) {
            backend.SetDebugFlag(registry, flag, enable);
        }
    }
}


// --- Visual Debugging Utilities ----------------------------------
entt::entity SpawnCameraFrustum(entt::registry& registry, entt::entity camera_entity, float visual_length) {
    if (!registry.valid(camera_entity)) return entt::null;

    auto model_variant = camera::GetCameraModel(registry, camera_entity);
    if (!std::holds_alternative<camera::PerspectiveModel>(model_variant)) {
        return entt::null; // Orthographic debug frustums not implemented yet
    }
    
    auto model = std::get<camera::PerspectiveModel>(model_variant);
    auto frustum_mesh = CreatePerspectiveCameraMesh(model, visual_length);
    

    // Register the Asset
    std::string mesh_id = "debug_frustum_" + std::to_string(static_cast<uint32_t>(camera_entity));
    entt::entity mesh_asset = data::CreateMesh(registry, mesh_id, std::move(frustum_mesh));
    data::BindMaterial(registry, mesh_asset, GetOrCreateWireframeDebugMaterial(registry));

    // Spawn Physical Entity and Mount it to the Camera
    entt::entity frustum_entity = registry.create();
    tf::AddTransform(registry, frustum_entity);
    tf::AttachChild(registry, camera_entity, frustum_entity, tf::TFDependencyPolicy::CASCADE_DELETE);
    
    // Bind to the debug layer
    data::BindMesh(registry, frustum_entity, mesh_asset, rrl::rhi::RenderLayer::LAYER_DEBUG);
    return frustum_entity;
}
entt::entity SpawnDebugGrid(entt::registry& registry, float size, int subdivisions) {
    data::MeshData grid_mesh;
    grid_mesh.topology = data::MeshTopology::LINES;

    float half_size = size / 2.0f;
    float step = size / static_cast<float>(subdivisions);

    uint32_t idx = 0;
    for (int i = 0; i <= subdivisions; ++i) {
        float offset = -half_size + (i * step);

        // Lines along X axis
        grid_mesh.positions.push_back({-half_size, offset, 0.0f});
        grid_mesh.positions.push_back({ half_size, offset, 0.0f});
        grid_mesh.indices.push_back(idx++);
        grid_mesh.indices.push_back(idx++);

        // Lines along Y axis
        grid_mesh.positions.push_back({offset, -half_size, 0.0f});
        grid_mesh.positions.push_back({offset,  half_size, 0.0f});
        grid_mesh.indices.push_back(idx++);
        grid_mesh.indices.push_back(idx++);
    }

    entt::entity mesh_asset = data::CreateMesh(registry, "debug_grid", std::move(grid_mesh));
    data::BindMaterial(registry, mesh_asset, GetOrCreateWireframeDebugMaterial(registry));
    entt::entity grid_entity = registry.create();
    tf::AddTransform(registry, grid_entity);
    
    // Bind to the debug layer
    data::BindMesh(registry, grid_entity, mesh_asset, rrl::rhi::RenderLayer::LAYER_DEBUG);
    return grid_entity;
}



} // namespace rrl::debug::rhi