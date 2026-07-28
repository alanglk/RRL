// RRL/tests/tf_module_tests.cpp

#include <gtest/gtest.h>
#include <memory>

#include <glm/glm.hpp>

#include <FLogging/FLogging.hpp>

// RRL Engine Modules
#include <RRL/RRLEngine.hpp>
#include <RRL/RRLTypes.hpp>
#include <RRL/tf/TFTypes.hpp>

class TransformTreeTest : public ::testing::Test {
protected:
    std::unique_ptr<rrl::Engine> engine;

    void SetUp() override {
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
        
        // Instantiating the engine perfectly spins up all ECS registries and sub-modules
        engine = std::make_unique<rrl::Engine>();
    }
    
    void TearDown() override {
        // Destroying the engine triggers the perfect RAII cascading teardown
        engine.reset();
        flogging::ResetLogger();
    }

    // Helper to easily extract position from a 4x4 matrix
    glm::vec3 ExtractPosition(const glm::mat4& matrix) {
        return glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
    }
};



// --- Basic TF API ------------------------------------------------
TEST_F(TransformTreeTest, RootNodeWorldMatrixUpdates) {
    rrl::ObjectID entity = engine->scene.SpawnObject();
    engine->tf.AddTransform(entity, glm::vec3(10.0f, 5.0f, -2.0f));

    // Tick
    engine->tf.UpdateTransformTree();

    glm::mat4 world_matrix = engine->tf.GetWorldMatrix(entity);
    glm::vec3 world_pos = ExtractPosition(world_matrix);

    EXPECT_FLOAT_EQ(world_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(world_pos.y, 5.0f);
    EXPECT_FLOAT_EQ(world_pos.z, -2.0f);

    // Verify internal state via Debugger
    auto report = engine->debug.GetTransformTreeDebugReport();
    ASSERT_EQ(report.root_nodes.size(), 1);
    EXPECT_EQ(report.root_nodes[0].depth, 0);
    EXPECT_FALSE(report.root_nodes[0].is_dirty) << "UpdateTransformTree should have cleared the dirty flag";
}

TEST_F(TransformTreeTest, DirtyFlagSkipsUnchangedBranches) {
    rrl::ObjectID entity = engine->scene.SpawnObject();
    engine->tf.AddTransform(entity, glm::vec3(0.0f));
    
    engine->tf.UpdateTransformTree();
    auto initial_report = engine->debug.GetTransformTreeDebugReport();
    uint32_t initial_version = initial_report.root_nodes[0].version;

    // Tick again WITHOUT moving the entity
    engine->tf.UpdateTransformTree();
    auto unchanged_report = engine->debug.GetTransformTreeDebugReport();
    
    // The version should NOT have incremented, proving the math was skipped
    EXPECT_EQ(unchanged_report.root_nodes[0].version, initial_version);

    // Move it, tick, and verify it updates
    engine->tf.SetLocalPosition(entity, glm::vec3(1.0f));
    engine->tf.UpdateTransformTree();
    
    auto changed_report = engine->debug.GetTransformTreeDebugReport();
    EXPECT_GT(changed_report.root_nodes[0].version, initial_version) << "Version should increment when node moves";
}

TEST_F(TransformTreeTest, ChildInheritsParentTransform) {
    rrl::ObjectID parent = engine->scene.SpawnObject();
    rrl::ObjectID child = engine->scene.SpawnObject();

    engine->tf.AddTransform(parent, glm::vec3(10.0f, 0.0f, 0.0f));
    engine->tf.AddTransform(child, parent, glm::vec3(5.0f, 0.0f, 0.0f));

    engine->tf.UpdateTransformTree();

    glm::mat4 child_world = engine->tf.GetWorldMatrix(child);
    glm::vec3 child_world_pos = ExtractPosition(child_world);

    EXPECT_FLOAT_EQ(child_world_pos.x, 15.0f);

    // Verify Hierarchy State
    auto report = engine->debug.GetTransformTreeDebugReport();
    ASSERT_EQ(report.root_nodes.size(), 1);
    ASSERT_EQ(report.root_nodes[0].children.size(), 1);
    
    auto& child_stats = report.root_nodes[0].children[0];
    EXPECT_EQ(child_stats.object_id, child);
    EXPECT_EQ(child_stats.depth, 1) << "Child should be at depth 1";
}


// --- Upward Propagation ------------------------------------------
TEST_F(TransformTreeTest, MovingParentMovesDeepHierarchy) {
    rrl::ObjectID grandparent = engine->scene.SpawnObject();
    rrl::ObjectID parent = engine->scene.SpawnObject();
    rrl::ObjectID child = engine->scene.SpawnObject();

    engine->tf.AddTransform(grandparent, glm::vec3(0.0f));
    engine->tf.AddTransform(parent, grandparent, glm::vec3(5.0f, 0.0f, 0.0f));
    engine->tf.AddTransform(child, parent, glm::vec3(5.0f, 0.0f, 0.0f));
    engine->tf.UpdateTransformTree();

    // Move the middle node (parent)
    engine->tf.SetLocalPosition(parent, glm::vec3(10.0f, 0.0f, 0.0f));
    
    // Verify dirty flag cascaded upward to the root
    auto pre_tick_report = engine->debug.GetTransformTreeDebugReport();
    EXPECT_TRUE(pre_tick_report.root_nodes[0].has_dirty_children) << "Path to root was not marked dirty!";
    
    // Verify the middle node itself was marked dirty locally
    EXPECT_TRUE(pre_tick_report.root_nodes[0].children[0].is_dirty);

    // Tick again
    engine->tf.UpdateTransformTree();

    // The deep child should have moved with the parent
    // GP(0) + P(10) + C(5) = 15.0f
    glm::vec3 child_world_pos = ExtractPosition(engine->tf.GetWorldMatrix(child));
    EXPECT_FLOAT_EQ(child_world_pos.x, 15.0f); 
}


// --- Structural Detachment ---------------------------------------
TEST_F(TransformTreeTest, DetachChildPreservesAbsoluteCoordinates) {
    rrl::ObjectID parent = engine->scene.SpawnObject();
    rrl::ObjectID child = engine->scene.SpawnObject();

    engine->tf.AddTransform(parent, glm::vec3(10.0f, 0.0f, 0.0f));
    engine->tf.AddTransform(child, parent, glm::vec3(0.0f, 5.0f, 0.0f));
    engine->tf.UpdateTransformTree();

    // Detach the child
    engine->tf.DetachChild(child);

    // Verify Graph Topology changed
    auto report = engine->debug.GetTransformTreeDebugReport();
    EXPECT_EQ(report.root_nodes.size(), 2) << "Child should have become a new root node!";
    
    // Verify local coordinates inherited absolute world coordinates
    glm::vec3 new_local_pos = engine->tf.GetLocalPosition(child);
    EXPECT_FLOAT_EQ(new_local_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(new_local_pos.y, 5.0f);

    // Tick and verify world matrix remains exactly the same
    engine->tf.UpdateTransformTree();
    glm::vec3 final_world_pos = ExtractPosition(engine->tf.GetWorldMatrix(child));
    EXPECT_FLOAT_EQ(final_world_pos.x, 10.0f);
    EXPECT_FLOAT_EQ(final_world_pos.y, 5.0f);
}


// --- Garbage Collection Policies ---------------------------------
TEST_F(TransformTreeTest, CascadeDeletePolicy) {
    rrl::ObjectID parent = engine->scene.SpawnObject();
    rrl::ObjectID child = engine->scene.SpawnObject();

    engine->tf.AddTransform(parent);
    engine->tf.AddTransform(child, parent, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f), rrl::tf::TFDependencyPolicy::CASCADE_DELETE);

    engine->scene.DestroyObject(parent);
    
    // Verify Cascade Delete using the Debug Report instead of raw EnTT registry
    auto report = engine->debug.GetTransformTreeDebugReport();
    EXPECT_EQ(report.total_valid_nodes, 0) << "Child was not destroyed via Cascade Delete!";
    EXPECT_EQ(report.root_nodes.size(), 0);
}

TEST_F(TransformTreeTest, ReparentToWorldPolicy) {
    rrl::ObjectID parent = engine->scene.SpawnObject();
    rrl::ObjectID child = engine->scene.SpawnObject();

    engine->tf.AddTransform(parent, glm::vec3(100.0f, 0.0f, 0.0f));
    engine->tf.AddTransform(child, parent, glm::vec3(0.0f, 0.0f, 2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f), rrl::tf::TFDependencyPolicy::REPARENT_TO_WORLD);
    
    engine->tf.UpdateTransformTree();

    engine->scene.DestroyObject(parent);

    // Verify it promoted to Root
    auto report = engine->debug.GetTransformTreeDebugReport();
    ASSERT_EQ(report.root_nodes.size(), 1) << "Child was improperly destroyed!";
    EXPECT_EQ(report.root_nodes[0].object_id, child);
    EXPECT_EQ(report.root_nodes[0].depth, 0);

    // Update the tree and check world matrix
    engine->tf.UpdateTransformTree();
    glm::vec3 world_pos = ExtractPosition(engine->tf.GetWorldMatrix(child));
    EXPECT_FLOAT_EQ(world_pos.x, 100.0f);
    EXPECT_FLOAT_EQ(world_pos.z, 2.0f);
}
