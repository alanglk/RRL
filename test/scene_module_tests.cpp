// RRL/tests/scene_module_tests.cpp

#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <random>

#include <FLogging/FLogging.hpp>

#include "RRL/scene/SceneManager.hpp"
#include "RRL/data/AssetManager.hpp"
#include "RRL/tf/TransformTree.hpp"

#include "RRL/debug/TFDebugger.hpp"
#include "RRL/debug/AssetManagerDebugger.hpp"
#include "RRL/debug/SceneManagerDebugger.hpp"

using namespace rrl;



class SceneManagerTest : public ::testing::Test {
protected:
    entt::registry registry;
    void SetUp() override {
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);

        data::InitializeAssetManager(registry);
        tf::RegisterTFActions(registry);
        scene::InitializeSceneManager(registry);
    }
    void TearDown() override {
        data::DestroyAllAssets(registry);
        registry.clear();
        flogging::ResetLogger();
    }

    // Dummy generators
    data::ImageData CreateDummyImage() {
        return {
            .width = 2,
            .height = 2,
            .channels = data::ImageChannelLayout::CH_3,
            .data = {255,0,0, 0,255,0, 0,0,255, 255,255,0}
        };
    }
    data::MeshData CreateDummyCuboid() {
        data::MeshData mesh;
        mesh.topology = data::MeshTopology::TRIANGLES;
        mesh.positions = {{0,0,0}, {1,0,0}, {0,1,0}};
        mesh.indices = {0, 1, 2};
        mesh.submeshes.push_back({0, 3}); 
        return mesh;
    }
    io::IOPrefab CreateDummyPrefab(const std::string& prefab_id) {
        io::IOPrefab prefab; 
        prefab.filepath = prefab_id;

        // dummy texture
        std::string tex_path = prefab_id + "_albedo";
        data::CreateTexture(registry, tex_path, CreateDummyImage());

        // dummy material
        io::IOMaterial mat;
        mat.name = "dummy_mat";
        mat.material_parameters.base_color = glm::vec4(1.0f);
        mat.albedo_path = tex_path; 
        prefab.materials.push_back(mat);

        // dummy geometry
        io::IONode chassis_node;
        chassis_node.name = "dummy_geom";
        chassis_node.mesh = CreateDummyCuboid();

        // Bind Material Hash to Mesh
        chassis_node.submesh_material_names.push_back("dummy_mat");

        prefab.root_nodes.push_back(std::move(chassis_node));
        return prefab;
    }
    
    io::IOPrefab CreateDynamicHierarchicalPrefab(
        const std::string& prefab_id,
        uint32_t num_textures,
        uint32_t num_materials,
        uint32_t num_meshes,
        uint32_t num_transform_nodes,
        uint32_t seed = 42) 
    {
        io::IOPrefab prefab;
        prefab.filepath = prefab_id;
        num_materials   = std::max(num_materials, num_textures); // materials should be greater than textures (use all textures)
        num_meshes      = std::max(num_meshes, num_materials);  // meshes shoudl be greater than materials (use all materials)
        

        // Generate Textures
        std::vector<std::string> tex_paths;
        for (uint32_t i = 0; i < num_textures; ++i) {
            std::string tex_path = prefab_id + "_tex_" + std::to_string(i);
            data::CreateTexture(registry, tex_path, CreateDummyImage());
            tex_paths.push_back(tex_path);
        }

        // Generate Materials
        std::vector<std::string> mat_names;
        for (uint32_t i = 0; i < num_materials; ++i) {
            io::IOMaterial mat;
            mat.name = "mat_" + std::to_string(i);
            mat.material_parameters.base_color = glm::vec4(1.0f);
            
            mat.albedo_path = tex_paths[i % num_textures]; // modulo to ensure every texture is used at least once
            prefab.materials.push_back(mat);
            mat_names.push_back(mat.name);
        }

        // Generate Meshes
        std::vector<data::MeshData> generated_meshes;
        std::vector<std::string> mesh_mat_names;
        for (uint32_t i = 0; i < num_meshes; ++i) {
            generated_meshes.push_back(CreateDummyCuboid());
            
            // Assign the material string identifier that pairs with this mesh
            mesh_mat_names.push_back(mat_names[i % num_materials]);
        }

        // Generate Flat Nodes
        uint32_t total_nodes = num_meshes + num_transform_nodes;
        std::vector<io::IONode> flat_nodes(total_nodes);
        for (uint32_t i = 0; i < total_nodes; ++i) {
            flat_nodes[i].name = "node_" + std::to_string(i);
            
            // The first 'num_meshes' nodes get geometry, the rest are pure transforms
            if (i < num_meshes) {
                flat_nodes[i].mesh = std::move(generated_meshes[i]);
            }
            // Give them all slightly offset transforms for debugging visibility
            flat_nodes[i].local_position = glm::vec3(static_cast<float>(i));
        }

        // Shuffle the nodes so pure transforms and mesh nodes are randomly dispersed in the tree
        std::mt19937 rng(seed);
        std::shuffle(flat_nodes.begin(), flat_nodes.end(), rng);


        // Tree Linking
        std::vector<std::vector<uint32_t>> children_map(total_nodes);
        std::vector<uint32_t> roots;
        for (uint32_t i = 0; i < total_nodes; ++i) {
            if (i == 0) {
                roots.push_back(i); // Node 0 is forced to be a root to guarantee a valid tree
            } else {
                // 20% chance to be a root, 80% chance to be a child
                std::uniform_int_distribution<uint32_t> root_dist(0, 9);
                if (root_dist(rng) < 2) {
                    roots.push_back(i);
                } else {
                    // Pick a random parent that comes strictly BEFORE this node.
                    // This completely eliminates the possibility of infinite recursion/cycles.
                    std::uniform_int_distribution<uint32_t> parent_dist(0, i - 1);
                    uint32_t parent_idx = parent_dist(rng);
                    children_map[parent_idx].push_back(i);
                }
            }
        }


        // Final IO Hierarchy
        // Recursive lambda to fold the flat list into the nested hierarchy
        std::function<io::IONode(uint32_t)> build_tree = [&](uint32_t idx) -> io::IONode {
            io::IONode node = std::move(flat_nodes[idx]);
            for (uint32_t child_idx : children_map[idx]) {
                node.children.push_back(build_tree(child_idx));
            }
            return node;
        };
        for (uint32_t root_idx : roots) {
            prefab.root_nodes.push_back(build_tree(root_idx));
        }
        return prefab;
    }
};


// --- Basic Blueprints API ----------------------------------------
TEST_F(SceneManagerTest, PreloadProceduralBlueprint) {
    std::string dummy_id = "dummy_prefab";
    auto dummy_prefab = CreateDummyPrefab(dummy_id);
    
    // Preload it into the cache
    scene::PreloadPrefabBlueprint(registry, dummy_id, std::move(dummy_prefab));

    // Check SceneManager Cache
    auto scene_report = debug::scene::GetSceneDebugReport(registry);
    ASSERT_EQ(scene_report.tracked_blueprints.size(), 1);
    EXPECT_EQ(scene_report.tracked_blueprints[dummy_id].live_instances_count, 0) << "Should have 0 spawned instances";
    EXPECT_EQ(scene_report.tracked_blueprints[dummy_id].root_nodes.size(), 1);

    // Verify AssetManager ingested the meshes and materials properly
    auto mesh_report    = debug::data::GetMeshDebugReport(registry);
    auto mat_report     = debug::data::GetMaterialDebugReport(registry);
    std::string expected_mesh_id    = dummy_id + "_mesh_dummy_geom";
    std::string expected_mat_id     = dummy_id + "_mat_dummy_mat";
    ASSERT_TRUE(mesh_report.tracked_assets.count(expected_mesh_id));
    ASSERT_TRUE(mat_report.tracked_assets.count(expected_mat_id));
    
    // The Blueprint itself pins the assets to keep them alive in RAM
    EXPECT_EQ(mesh_report.tracked_assets[expected_mesh_id].ref_count, 1);
}
TEST_F(SceneManagerTest, SpawnSpecificSubNode) {
    // Create a custom multi-level prefab to test the path parser
    io::IOPrefab house_prefab;
    house_prefab.filepath = "house_model";
    
    io::IONode root_node;
    root_node.name = "building"; // Top level node
    
    io::IONode child_node;
    child_node.name = "door";    // Child level node
    child_node.mesh = CreateDummyCuboid(); // Only the door has geometry
    
    root_node.children.push_back(std::move(child_node));
    house_prefab.root_nodes.push_back(std::move(root_node));

    std::string bp_id = "city_house";
    scene::PreloadPrefabBlueprint(registry, bp_id, std::move(house_prefab));

    // We only want to spawn the door, ignoring the rest of the building!
    // Format: BlueprintID.RootNode.ChildNode
    entt::entity lone_door = scene::SpawnPrefab(registry, "city_house.building.door");
    ASSERT_TRUE(lone_door != entt::null);
    
    // Verify it received the physical Mesh Linkage
    auto mesh_report = debug::data::GetMeshDebugReport(registry);
    std::string door_mesh_id = bp_id + "_mesh_door";
    
    ASSERT_TRUE(mesh_report.tracked_assets.count(door_mesh_id));
    EXPECT_EQ(mesh_report.tracked_assets[door_mesh_id].ref_count, 2); // 1 template + 1 instance
}



TEST_F(SceneManagerTest, ComplexPrefabHierarchyTest) {
    std::string prefab_id = "tree";
    
    // Generate a massive tree with 5 textures, 10 materials, 25 meshes, and 25 pure transforms
    auto heavy_prefab = CreateDynamicHierarchicalPrefab(
        prefab_id, 
        5, 10, 25, 25, 
        1337 // Fixed seed for reproducible tests
    );
    scene::PreloadPrefabBlueprint(registry, prefab_id, std::move(heavy_prefab));

    // Spawn 100 copies of this massive tree
    std::vector<entt::entity> swarm;
    for (int i = 0; i < 100; ++i) {
        swarm.push_back(scene::SpawnPrefab(registry, prefab_id));
    }

    // Check AssetManager (Instances share memory)
    auto tex_report = debug::data::GetTextureDebugReport(registry);
    auto mat_report = debug::data::GetMaterialDebugReport(registry);
    auto mesh_report = debug::data::GetMeshDebugReport(registry);
    EXPECT_EQ(tex_report.tracked_assets.size(), 5);
    EXPECT_EQ(mat_report.tracked_assets.size(), 10);
    EXPECT_EQ(mesh_report.tracked_assets.size(), 25);


    // Check the TF generated Tree
    // (50 internal nodes + 1 instance root) * 100 instances
    auto tf_report = debug::tf::GetTransformTreeDebugReport(registry);
    EXPECT_EQ(tf_report.malformed_nodes.size(), 0) << "SceneManager spawned malformed transforms";
    EXPECT_EQ(tf_report.cyclical_nodes.size(), 0) << "Topological generator created impossible loops";
    EXPECT_EQ(tf_report.root_nodes.size(), 100);
    EXPECT_EQ(tf_report.total_valid_nodes, 5100);


    // Drestroy the prefab with asset_deletion enabled -> force cache wipe
    for (auto instance : swarm) {
        scene::DestroyPrefab(registry, instance, true);
    }


    // Check GB
    EXPECT_EQ(debug::data::GetTextureDebugReport(registry).tracked_assets.size(), 0) << "Textures Leaked";
    EXPECT_EQ(debug::data::GetMaterialDebugReport(registry).tracked_assets.size(), 0) << "Materials Leaked";
    EXPECT_EQ(debug::data::GetMeshDebugReport(registry).tracked_assets.size(), 0) << "Meshes Leaked";
    auto tf_report_after = debug::tf::GetTransformTreeDebugReport(registry);
    EXPECT_EQ(tf_report_after.total_valid_nodes, 0) << "Transform entities leaked! Cascade delete failed.";
}
