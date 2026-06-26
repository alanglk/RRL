// RRL/tests/tf_module_tests.cpp

#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <FLogging/FLogging.hpp>

#include "RRL/tf/TransformTree.hpp"
#include "RRL/debug/TFDebugger.hpp"

using namespace rrl::tf;
using namespace rrl::debug::tf;



class TransformTreeTest : public ::testing::Test {
protected:
    entt::registry registry;

    void SetUp() override {
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
        RegisterTFActions(registry);
    }
    
    void TearDown() override {
        flogging::ResetLogger();
    }

    // Helper to easily extract position from a 4x4 matrix
    glm::vec3 ExtractPosition(const glm::mat4& matrix) {
        return glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
    }
};



// --- Basic TF API ------------------------------------------------
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

    // Verify internal state via Debugger
    auto report = GetTransformTreeDebugReport(registry);
    ASSERT_EQ(report.root_nodes.size(), 1);
    EXPECT_EQ(report.root_nodes[0].depth, 0);
    EXPECT_FALSE(report.root_nodes[0].is_dirty) << "UpdateTransformTree should have cleared the dirty flag";
}
TEST_F(TransformTreeTest, DirtyFlagSkipsUnchangedBranches) {
    auto entity = registry.create();
    AddTransform(registry, entity, glm::vec3(0.0f));
    
    UpdateTransformTree(registry);
    auto initial_report = GetTransformTreeDebugReport(registry);
    uint32_t initial_version = initial_report.root_nodes[0].version;

    // Tick again WITHOUT moving the entity
    UpdateTransformTree(registry);
    auto unchanged_report = GetTransformTreeDebugReport(registry);
    
    // The version should NOT have incremented, proving the math was skipped
    EXPECT_EQ(unchanged_report.root_nodes[0].version, initial_version);

    // Move it, tick, and verify it updates
    SetLocalPosition(registry, entity, glm::vec3(1.0f));
    UpdateTransformTree(registry);
    
    auto changed_report = GetTransformTreeDebugReport(registry);
    EXPECT_GT(changed_report.root_nodes[0].version, initial_version) << "Version should increment when node moves";
}
TEST_F(TransformTreeTest, ChildInheritsParentTransform) {
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, parent, glm::vec3(10.0f, 0.0f, 0.0f));
    AddTransform(registry, child, parent, glm::vec3(5.0f, 0.0f, 0.0f));

    UpdateTransformTree(registry);

    glm::mat4 child_world = GetWorldMatrix(registry, child);
    glm::vec3 child_world_pos = ExtractPosition(child_world);

    EXPECT_FLOAT_EQ(child_world_pos.x, 15.0f);

    // Verify Hierarchy State
    auto report = GetTransformTreeDebugReport(registry);
    ASSERT_EQ(report.root_nodes.size(), 1);
    ASSERT_EQ(report.root_nodes[0].children.size(), 1);
    
    auto& child_stats = report.root_nodes[0].children[0];
    EXPECT_EQ(child_stats.entity_id, child);
    EXPECT_EQ(child_stats.depth, 1) << "Child should be at depth 1";
}


// --- Upward Propagation ------------------------------------------
TEST_F(TransformTreeTest, MovingParentMovesDeepHierarchy) {
    auto grandparent = registry.create();
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, grandparent, glm::vec3(0.0f));
    AddTransform(registry, parent, grandparent, glm::vec3(5.0f, 0.0f, 0.0f));
    AddTransform(registry, child, parent, glm::vec3(5.0f, 0.0f, 0.0f));
    UpdateTransformTree(registry);

    // Move the middle node (parent)
    SetLocalPosition(registry, parent, glm::vec3(10.0f, 0.0f, 0.0f));
    
    // Verify dirty flag cascaded upward to the root
    auto pre_tick_report = GetTransformTreeDebugReport(registry);
    EXPECT_TRUE(pre_tick_report.root_nodes[0].has_dirty_children) << "Path to root was not marked dirty!";
    
    // Verify the middle node itself was marked dirty locally
    EXPECT_TRUE(pre_tick_report.root_nodes[0].children[0].is_dirty);

    // Tick again
    UpdateTransformTree(registry);

    // The deep child should have moved with the parent
    // GP(0) + P(10) + C(5) = 15.0f
    glm::vec3 child_world_pos = ExtractPosition(GetWorldMatrix(registry, child));
    EXPECT_FLOAT_EQ(child_world_pos.x, 15.0f); 
}


// --- Structural Detachment ---------------------------------------
TEST_F(TransformTreeTest, DetachChildPreservesAbsoluteCoordinates) {
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, parent, glm::vec3(10.0f, 0.0f, 0.0f));
    AddTransform(registry, child, parent, glm::vec3(0.0f, 5.0f, 0.0f));
    UpdateTransformTree(registry);

    // Detach the child
    DetachChild(registry, child);

    // Verify Graph Topology changed
    auto report = GetTransformTreeDebugReport(registry);
    EXPECT_EQ(report.root_nodes.size(), 2) << "Child should have become a new root node!";
    
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

    registry.destroy(parent);

    EXPECT_FALSE(registry.valid(child)) << "Child was not destroyed via Cascade Delete!";
    
    auto report = GetTransformTreeDebugReport(registry);
    EXPECT_EQ(report.total_valid_nodes, 0);
}
TEST_F(TransformTreeTest, ReparentToWorldPolicy) {
    auto parent = registry.create();
    auto child = registry.create();

    AddTransform(registry, parent, glm::vec3(100.0f, 0.0f, 0.0f));
    AddTransform(registry, child, parent, glm::vec3(0.0f, 0.0f, 2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f), TFDependencyPolicy::REPARENT_TO_WORLD);
    
    UpdateTransformTree(registry);

    registry.destroy(parent);

    EXPECT_TRUE(registry.valid(child)) << "Child was improperly destroyed!";

    // Verify it promoted to Root
    auto report = GetTransformTreeDebugReport(registry);
    ASSERT_EQ(report.root_nodes.size(), 1);
    EXPECT_EQ(report.root_nodes[0].entity_id, child);
    EXPECT_EQ(report.root_nodes[0].depth, 0);

    // Update the tree and check world matrix
    UpdateTransformTree(registry);
    glm::vec3 world_pos = ExtractPosition(GetWorldMatrix(registry, child));
    EXPECT_FLOAT_EQ(world_pos.x, 100.0f);
    EXPECT_FLOAT_EQ(world_pos.z, 2.0f);
}

