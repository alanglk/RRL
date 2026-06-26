// RRL/tests/camera_module_tests.cpp

#include <gtest/gtest.h>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/epsilon.hpp>

#include <FLogging/FLogging.hpp>

#include "RRL/tf/TransformTree.hpp"
#include "RRL/camera/CameraSystem.hpp"

using namespace rrl;



class CameraModuleTest : public ::testing::Test {
protected:
    entt::registry registry;
    
    // OpenGL conventions as baseline for testing projections
    camera::NDCConvention test_ndc = { 
        camera::NDCDepth::MINUS_ONE_TO_ONE, 
        camera::NDCYDirection::UP 
    };

    void SetUp() override {
        flogging::InitLogger(flogging::LogLevel::Warn, flogging::BackendType::StdFormat);
        tf::RegisterTFActions(registry);
    }

    // Helper to easily extract position from a 4x4 matrix
    glm::vec3 ExtractPosition(const glm::mat4& matrix) {
        return glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
    }

    // Helper to check if two matrices are equal within a small float tolerance
    bool MatricesAreNear(const glm::mat4& m1, const glm::mat4& m2, float tolerance = 1e-5f) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (std::abs(m1[i][j] - m2[i][j]) > tolerance) return false;
            }
        }
        return true;
    }
};



// --- Lifecycle ---------------------------------------------------

TEST_F(CameraModuleTest, PrimaryCameraManagement) {
    // New API automatically creates entities and registers components
    auto cam1 = camera::SpawnCamera(registry, camera::PerspectiveModel{}, rhi::TARGET_MAIN);
    auto cam2 = camera::SpawnCamera(registry, camera::PerspectiveModel{}, rhi::TARGET_NULL);

    EXPECT_EQ(camera::GetPrimaryCamera(registry), cam1);

    // Switch primary to cam2
    camera::SetPrimaryCamera(registry, cam2);

    EXPECT_EQ(camera::GetPrimaryCamera(registry), cam2);
    // Ensure cam1 was automatically unset by the internal target manager
    EXPECT_NE(camera::GetPrimaryCamera(registry), cam1); 
}

TEST_F(CameraModuleTest, ModelUpdatingAndRetrieval) {
    auto entity = camera::SpawnCamera(registry, camera::PerspectiveModel{});

    // Retrieve copy, modify, and set
    auto variant = camera::GetCameraModel(registry, entity);
    auto* persp = std::get_if<camera::PerspectiveModel>(&variant);
    ASSERT_NE(persp, nullptr);
    
    persp->fov_y_radians = 2.0f; // Modify
    camera::SetCameraModel(registry, entity, variant);

    // Retrieve again to verify the change was saved
    auto updated_variant = camera::GetCameraModel(registry, entity);
    auto* updated_persp = std::get_if<camera::PerspectiveModel>(&updated_variant);
    
    EXPECT_FLOAT_EQ(updated_persp->fov_y_radians, 2.0f);
}



// --- View Math ---------------------------------------------------

TEST_F(CameraModuleTest, ViewMatrixLCS_to_CCS_Translation) {
    auto cam_entity = camera::SpawnCamera(registry);
    
    // World Position: Z = 5 (Up in ISO 8855)
    tf::SetLocalPosition(registry, cam_entity, glm::vec3(0.0f, 0.0f, 5.0f));

    tf::UpdateTransformTree(registry);
    camera::UpdateCameras(registry, test_ndc);

    glm::mat4 view_matrix = camera::GetViewMatrix(registry, cam_entity);
    glm::vec3 ccs_pos = ExtractPosition(view_matrix);

    // Inverse world translation is (0, 0, -5).
    // Mapping LCS to CCS:
    // CCS X (Right) = LCS -Y = 0
    // CCS Y (Up)    = LCS +Z = -5.0f
    // CCS Z (Back)  = LCS -X = 0
    EXPECT_FLOAT_EQ(ccs_pos.x, 0.0f);
    EXPECT_FLOAT_EQ(ccs_pos.y, -5.0f);
    EXPECT_FLOAT_EQ(ccs_pos.z, 0.0f);
}



// --- Projection Math ---------------------------------------------

TEST_F(CameraModuleTest, ProjectionUpdatesOnModelChange) {
    auto cam_entity = camera::SpawnCamera(registry, camera::PerspectiveModel{});
    
    tf::UpdateTransformTree(registry);
    camera::UpdateCameras(registry, test_ndc);
    
    glm::mat4 perspective_proj = camera::GetProjectionMatrix(registry, cam_entity);

    // Switch to Orthographic
    camera::SetCameraModel(registry, cam_entity, camera::OrthographicModel{});
    camera::UpdateCameras(registry, test_ndc);
    
    glm::mat4 ortho_proj = camera::GetProjectionMatrix(registry, cam_entity);

    // The projection matrix should have been rebuilt 
    EXPECT_FALSE(MatricesAreNear(perspective_proj, ortho_proj));
}

TEST_F(CameraModuleTest, PinholeDepthMapping) {
    camera::PinholeModel pinhole;
    pinhole.z_near = 1.0f;
    pinhole.z_far = 100.0f;
    auto cam_entity = camera::SpawnCamera(registry, pinhole);

    tf::UpdateTransformTree(registry);
    
    // Test with OpenCV conventions (ZERO_TO_ONE depth)
    camera::NDCConvention cv_ndc = { camera::NDCDepth::ZERO_TO_ONE, camera::NDCYDirection::DOWN };
    camera::UpdateCameras(registry, cv_ndc);

    glm::mat4 proj = camera::GetProjectionMatrix(registry, cam_entity);

    // For ZERO_TO_ONE depth mapping, the [2][2] and [3][2] elements handle Z-normalization
    // M[2][2] = z_far / (z_far - z_near) = 100 / 99 = 1.0101...
    EXPECT_NEAR(proj[2][2], 100.0f / 99.0f, 1e-4f);
    
    // M[3][2] = -(z_far * z_near) / (z_far - z_near) = -100 / 99 = -1.0101...
    EXPECT_NEAR(proj[3][2], -100.0f / 99.0f, 1e-4f);
}



// --- Dynamic Cameras ---------------------------------------------

TEST_F(CameraModuleTest, CameraInheritsParentMovement) {
    auto drone = registry.create();
    // Drone at X = 10
    tf::AddTransform(registry, drone, glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    
    // Spawn camera using new API
    auto camera = camera::SpawnCamera(registry);
    
    // Mount the camera Z = 2 (above the drone)
    tf::AttachChild(registry, drone, camera, tf::TFDependencyPolicy::CASCADE_DELETE);
    tf::SetLocalPosition(registry, camera, glm::vec3(0.0f, 0.0f, 2.0f));

    // Initial tick
    tf::UpdateTransformTree(registry);
    camera::UpdateCameras(registry, test_ndc);

    glm::mat4 initial_view = camera::GetViewMatrix(registry, camera);
    glm::vec3 initial_ccs_pos = ExtractPosition(initial_view);

    // World = (10, 0, 2). Inverse = (-10, 0, -2).
    // CCS mapping: X = -Y, Y = Z, Z = -X
    EXPECT_FLOAT_EQ(initial_ccs_pos.x, 0.0f);
    EXPECT_FLOAT_EQ(initial_ccs_pos.y, -2.0f);
    EXPECT_FLOAT_EQ(initial_ccs_pos.z, 10.0f);

    // Move the Drone
    tf::SetLocalPosition(registry, drone, glm::vec3(20.0f, 0.0f, 0.0f));
    
    // Tick again
    tf::UpdateTransformTree(registry);
    camera::UpdateCameras(registry, test_ndc);

    glm::mat4 new_view = camera::GetViewMatrix(registry, camera);
    glm::vec3 new_ccs_pos = ExtractPosition(new_view);

    // World is now (20, 0, 2). Inverse = (-20, 0, -2).
    // CCS Z (Back) = LCS -X
    // Since X is -20, CCS Z becomes +20
    EXPECT_FLOAT_EQ(new_ccs_pos.z, 20.0f);
}