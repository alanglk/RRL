// RRL/tests/camera_module_tests.cpp

#include <gtest/gtest.h>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/epsilon.hpp>

#include <FLogging/FLogging.hpp>

// RRL Engine Modules
#include <RRL/RRLEngine.hpp>
#include <RRL/RRLTypes.hpp>
#include <RRL/camera/CameraModels.hpp>
#include <RRL/camera/CameraConventions.hpp>


class CameraModuleTest : public ::testing::Test {
protected:
    std::unique_ptr<rrl::Engine> engine;
    
    // OpenGL conventions as baseline for testing projections
    rrl::camera::NDCConvention test_ndc = { 
        rrl::camera::NDCDepth::MINUS_ONE_TO_ONE, 
        rrl::camera::NDCYDirection::UP 
    };

    void SetUp() override {
        flogging::InitLogger(flogging::LogLevel::Warn, flogging::BackendType::StdFormat);
        engine = std::make_unique<rrl::Engine>();
    }

    void TearDown() override {
        engine.reset();
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
    rrl::ObjectID cam1 = engine->camera.SpawnCamera(rrl::camera::PerspectiveModel{}, rrl::rhi::TARGET_MAIN);
    rrl::ObjectID cam2 = engine->camera.SpawnCamera(rrl::camera::PerspectiveModel{}, rrl::rhi::TARGET_NULL);

    EXPECT_EQ(engine->camera.GetPrimaryCamera(), cam1);

    // Switch primary to cam2
    engine->camera.SetPrimaryCamera(cam2);

    EXPECT_EQ(engine->camera.GetPrimaryCamera(), cam2);
    // Ensure cam1 was automatically unset by the internal target manager
    EXPECT_NE(engine->camera.GetPrimaryCamera(), cam1); 
}

TEST_F(CameraModuleTest, ModelUpdatingAndRetrieval) {
    rrl::ObjectID entity = engine->camera.SpawnCamera(rrl::camera::PerspectiveModel{});

    // Retrieve copy, modify, and set
    auto variant = engine->camera.GetCameraModel(entity);
    auto* persp = std::get_if<rrl::camera::PerspectiveModel>(&variant);
    ASSERT_NE(persp, nullptr);
    
    persp->fov_y_radians = 2.0f; // Modify
    engine->camera.SetCameraModel(entity, variant);

    // Retrieve again to verify the change was saved
    auto updated_variant = engine->camera.GetCameraModel(entity);
    auto* updated_persp = std::get_if<rrl::camera::PerspectiveModel>(&updated_variant);
    
    EXPECT_FLOAT_EQ(updated_persp->fov_y_radians, 2.0f);
}



// --- View Math ---------------------------------------------------

TEST_F(CameraModuleTest, ViewMatrixLCS_to_CCS_Translation) {
    rrl::ObjectID cam_entity = engine->camera.SpawnCamera();
    
    // World Position: Z = 5 (Up in ISO 8855)
    engine->tf.SetLocalPosition(cam_entity, glm::vec3(0.0f, 0.0f, 5.0f));

    engine->tf.UpdateTransformTree();
    engine->camera.UpdateCameras(test_ndc);

    glm::mat4 view_matrix = engine->camera.GetViewMatrix(cam_entity);
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
    rrl::ObjectID cam_entity = engine->camera.SpawnCamera(rrl::camera::PerspectiveModel{});
    
    engine->tf.UpdateTransformTree();
    engine->camera.UpdateCameras(test_ndc);
    
    glm::mat4 perspective_proj = engine->camera.GetProjectionMatrix(cam_entity);

    // Switch to Orthographic
    engine->camera.SetCameraModel(cam_entity, rrl::camera::OrthographicModel{});
    engine->camera.UpdateCameras(test_ndc);
    
    glm::mat4 ortho_proj = engine->camera.GetProjectionMatrix(cam_entity);

    // The projection matrix should have been rebuilt 
    EXPECT_FALSE(MatricesAreNear(perspective_proj, ortho_proj));
}

TEST_F(CameraModuleTest, PinholeDepthMapping) {
    rrl::camera::PinholeModel pinhole;
    pinhole.z_near = 1.0f;
    pinhole.z_far = 100.0f;
    rrl::ObjectID cam_entity = engine->camera.SpawnCamera(pinhole);

    engine->tf.UpdateTransformTree();
    
    // Test with OpenCV conventions (ZERO_TO_ONE depth)
    rrl::camera::NDCConvention cv_ndc = { rrl::camera::NDCDepth::ZERO_TO_ONE, rrl::camera::NDCYDirection::DOWN };
    engine->camera.UpdateCameras(cv_ndc);

    glm::mat4 proj = engine->camera.GetProjectionMatrix(cam_entity);

    // For ZERO_TO_ONE depth mapping, the [2][2] and [3][2] elements handle Z-normalization
    // M[2][2] = z_far / (z_far - z_near) = 100 / 99 = 1.0101...
    EXPECT_NEAR(proj[2][2], 100.0f / 99.0f, 1e-4f);
    
    // M[3][2] = -(z_far * z_near) / (z_far - z_near) = -100 / 99 = -1.0101...
    EXPECT_NEAR(proj[3][2], -100.0f / 99.0f, 1e-4f);
}



// --- Dynamic Cameras ---------------------------------------------

TEST_F(CameraModuleTest, CameraInheritsParentMovement) {
    rrl::ObjectID drone = engine->scene.SpawnObject();
    // Drone at X = 10
    engine->tf.AddTransform(drone, glm::vec3(10.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
    
    // Spawn camera using new API
    rrl::ObjectID camera = engine->camera.SpawnCamera();
    
    // Mount the camera Z = 2 (above the drone)
    engine->tf.AttachChild(drone, camera, rrl::tf::TFDependencyPolicy::CASCADE_DELETE);
    engine->tf.SetLocalPosition(camera, glm::vec3(0.0f, 0.0f, 2.0f));

    // Initial tick
    engine->tf.UpdateTransformTree();
    engine->camera.UpdateCameras(test_ndc);

    glm::mat4 initial_view = engine->camera.GetViewMatrix(camera);
    glm::vec3 initial_ccs_pos = ExtractPosition(initial_view);

    // World = (10, 0, 2). Inverse = (-10, 0, -2).
    // CCS mapping: X = -Y, Y = Z, Z = -X
    EXPECT_FLOAT_EQ(initial_ccs_pos.x, 0.0f);
    EXPECT_FLOAT_EQ(initial_ccs_pos.y, -2.0f);
    EXPECT_FLOAT_EQ(initial_ccs_pos.z, 10.0f);

    // Move the Drone
    engine->tf.SetLocalPosition(drone, glm::vec3(20.0f, 0.0f, 0.0f));
    
    // Tick again
    engine->tf.UpdateTransformTree();
    engine->camera.UpdateCameras(test_ndc);

    glm::mat4 new_view = engine->camera.GetViewMatrix(camera);
    glm::vec3 new_ccs_pos = ExtractPosition(new_view);

    // World is now (20, 0, 2). Inverse = (-20, 0, -2).
    // CCS Z (Back) = LCS -X
    // Since X is -20, CCS Z becomes +20
    EXPECT_FLOAT_EQ(new_ccs_pos.z, 20.0f);
}