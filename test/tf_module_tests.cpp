#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <FLogging/FLogging.hpp>

#include "RRL/tf/TransformTree.hpp"

using namespace rrl::tf;



class TransformTreeTest : public ::testing::Test {
protected:
    entt::registry registry;

    void SetUp() override {
        flogging::InitLogger(flogging::LogLevel::Warn, flogging::BackendType::StdFormat);
        RegisterTFActions(registry);
    }

    // Helper to easily extract position from a 4x4 matrix
    glm::vec3 ExtractPosition(const glm::mat4& matrix) {
        return glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
    }
};



// --- Updates -----------------------------------------------------

TEST_F(TransformTreeTest, RootNodeWorldMatrixUpdates) {
    auto entity = registry.create();
    AddTransform(registry, entity, glm::vec3(10.0f, 5.0f, -2.0f));

    // Tick
    UpdateTransformTree(registry);

    glm::mat4 world_matrix = GetWorldMatrix(registry, entity);
    glm::vec3 world_pos = ExtractPosition(world_matrix);

    EXPECT_FLOAT_EQ(world_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(world_pos.y, 5.0f);
    EXPECT_FLOAT_EQ(world_pos.z, -2.0f);
}

TEST_F(TransformTreeTest, ChildInheritsParentTransform) {
    auto parent = registry.create();
    auto child = registry.create();

    // Parent is at X = 10
    AddTransform(registry, parent, glm::vec3(10.0f, 0.0f, 0.0f));
    
    // Child is relative to parent at X = 5
    AddTransform(registry, child, parent, glm::vec3(5.0f, 0.0f, 0.0f));

    UpdateTransformTree(registry);

    glm::mat4 child_world = GetWorldMatrix(registry, child);
    glm::vec3 child_world_pos = ExtractPosition(child_world);

    // Absolute world position should be 10 + 5 = 15
    EXPECT_FLOAT_EQ(child_world_pos.x, 15.0f);
    EXPECT_FLOAT_EQ(child_world_pos.y, 0.0f);
    EXPECT_FLOAT_EQ(child_world_pos.z, 0.0f);
}



// --- Upward Propagation ------------------------------------------

TEST_F(TransformTreeTest, MovingParentMovesChild) {
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, parent, glm::vec3(0.0f));
    AddTransform(registry, child, parent, glm::vec3(5.0f, 0.0f, 0.0f));
    UpdateTransformTree(registry);

    // Move the parent after initial tick
    SetLocalPosition(registry, parent, glm::vec3(10.0f, 0.0f, 0.0f));
    
    // Tick again
    UpdateTransformTree(registry);

    glm::vec3 child_world_pos = ExtractPosition(GetWorldMatrix(registry, child));

    // The child should have moved with the parent
    EXPECT_FLOAT_EQ(child_world_pos.x, 15.0f);
}



// --- Structural Detachment ---------------------------------------

TEST_F(TransformTreeTest, DetachChildPreservesAbsoluteCoordinates) {
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, parent, glm::vec3(10.0f, 0.0f, 0.0f));
    AddTransform(registry, child, parent, glm::vec3(0.0f, 5.0f, 0.0f));
    UpdateTransformTree(registry);

    // Detach the child (should bake World coords into Local coords)
    DetachChild(registry, child);

    // Verify local coordinates inherited absolute world coordinates
    glm::vec3 new_local_pos = GetLocalPosition(registry, child);
    EXPECT_FLOAT_EQ(new_local_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(new_local_pos.y, 5.0f);

    // Tick and verify world matrix remains exactly the same
    UpdateTransformTree(registry);
    glm::vec3 final_world_pos = ExtractPosition(GetWorldMatrix(registry, child));
    EXPECT_FLOAT_EQ(final_world_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(final_world_pos.y, 5.0f);
}



// --- Garbage Collection Policies ---------------------------------

TEST_F(TransformTreeTest, CascadeDeletePolicy) {
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, parent);
    AddTransform(registry, child, parent, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f), TFDependencyPolicy::CASCADE_DELETE);

    // Verify child exists
    EXPECT_TRUE(registry.valid(child));

    // Destroy parent
    registry.destroy(parent);

    // Verify child is also destroyed automatically
    EXPECT_FALSE(registry.valid(child));
}

TEST_F(TransformTreeTest, ReparentToWorldPolicy) {
    auto parent = registry.create();
    auto child = registry.create();

    // Moving Car
    AddTransform(registry, parent, glm::vec3(100.0f, 0.0f, 0.0f));
    
    // Dropped box
    AddTransform(registry, child, parent, glm::vec3(0.0f, 0.0f, 2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f), TFDependencyPolicy::REPARENT_TO_WORLD);
    
    UpdateTransformTree(registry);

    // Destroy the car
    registry.destroy(parent);

    // The box should STILL exist
    EXPECT_TRUE(registry.valid(child));

    // The box's local position should now be the absolute world position
    glm::vec3 local_pos = GetLocalPosition(registry, child);
    EXPECT_FLOAT_EQ(local_pos.x, 100.0f);
    EXPECT_FLOAT_EQ(local_pos.y, 0.0f);
    EXPECT_FLOAT_EQ(local_pos.z, 2.0f);

    // Update the tree to ensure the new world matrix is calculated properly
    UpdateTransformTree(registry);
    glm::vec3 world_pos = ExtractPosition(GetWorldMatrix(registry, child));
    EXPECT_FLOAT_EQ(world_pos.x, 100.0f);
    EXPECT_FLOAT_EQ(world_pos.z, 2.0f);
}