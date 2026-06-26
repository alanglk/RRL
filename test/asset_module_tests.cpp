// RRL/tests/asset_module_tests.cpp

#include <gtest/gtest.h>
#include <entt/entt.hpp>

#include <FLogging/FLogging.hpp>

#include "RRL/data/AssetManager.hpp"
#include "RRL/debug/AssetManagerDebugger.hpp"

using namespace rrl::data;
using namespace rrl::debug::data;


class AssetManagerTest : public ::testing::Test {
protected:
    entt::registry registry;
    void SetUp() override {
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);

        InitializeAssetManager(registry);
    }
    void TearDown() override {
        DestroyAllAssets(registry);
        registry.clear();
        flogging::ResetLogger();
    }

    // Dummy generators
    ImageData CreateDummyImage() {
        return {
            .width = 1,
            .height = 1,
            .channels = ImageChannelLayout::CH_3,
            .data = {255, 0, 0} // 1 red pixel
        };
    }
    MeshData CreateDummyMesh() {
        return {
            .topology = MeshTopology::TRIANGLES,
            .positions = {{0,0,0}, {1,0,0}, {0,1,0}},
            .indices = {0, 1, 2}
        };
    }
    MaterialData CreateDummyMaterial() {
        return {
            .base_color = {1.0f, 1.0f, 1.0f, 1.0f}
        };
    }
};



// --- Basic Asset API ---------------------------------------------
TEST_F(AssetManagerTest, CreateAndCacheTexture) {
    entt::entity tex = CreateTexture(registry, "tex_diffuse", CreateDummyImage());
    
    // Assets texture report
    auto report = GetTextureDebugReport(registry);
    EXPECT_EQ(report.leaked_assets.size(), 0) << "Found leaked textures in the registry";
    EXPECT_EQ(report.dead_assets.size(), 0) << "Found dead string IDs in the cache";
    ASSERT_EQ(report.tracked_assets.size(), 1);

    // Verify Asset Stats
    auto& stats = report.tracked_assets["tex_diffuse"];
    EXPECT_EQ(stats.entity_id, tex);
    EXPECT_EQ(stats.ref_count, 0) << "New assets should start with 0 references";
}
TEST_F(AssetManagerTest, CascadeDeleteOnZeroRefs) {
    entt::entity tex = CreateTexture(registry, "tex_temp", CreateDummyImage());
    
    IncrementAssetRef(registry, tex);
    EXPECT_EQ(GetTextureDebugReport(registry).tracked_assets["tex_temp"].ref_count, 1);

    // Since the default policy is CASCADE_DELETE, dropping to 0 should destroy it instantly
    DecrementAssetRef(registry, tex);
    
    auto report = GetTextureDebugReport(registry);
    EXPECT_EQ(report.tracked_assets.size(), 0) << "Texture was not destroyed after ref count hit 0";
    EXPECT_EQ(report.leaked_assets.size(), 0);
    EXPECT_EQ(report.dead_assets.size(), 0);
}
TEST_F(AssetManagerTest, OverrideCachedAsset) {
    entt::entity first_tex = CreateTexture(registry, "tex_shared", CreateDummyImage());
    
    // Override the ID with a new texture
    entt::entity second_tex = CreateTexture(registry, "tex_shared", CreateDummyImage());

    EXPECT_NE(first_tex, second_tex);
    EXPECT_FALSE(registry.valid(first_tex)) << "The first texture was not destroyed when overridden!";
    
    auto report = GetTextureDebugReport(registry);
    EXPECT_EQ(report.tracked_assets.size(), 1);
    EXPECT_EQ(report.tracked_assets["tex_shared"].entity_id, second_tex);
    EXPECT_EQ(report.leaked_assets.size(), 0);
}


// --- Asset GB Dependencies ---------------------------------------
TEST_F(AssetManagerTest, KeepCachedAssetsPolicy) {
    SetAssetGCPolicy(registry, AssetGCPolicy::KEEP_CACHED_ASSETS);
    
    entt::entity tex = CreateTexture(registry, "tex_keep", CreateDummyImage());
    IncrementAssetRef(registry, tex);
    DecrementAssetRef(registry, tex);

    // It should STILL exist in the registry with 0 refs
    auto report_before = GetTextureDebugReport(registry);
    ASSERT_EQ(report_before.tracked_assets.size(), 1);
    EXPECT_EQ(report_before.tracked_assets["tex_keep"].ref_count, 0);

    // Manually flush unused assets
    FreeUnusedAssets(registry);

    // Now it should be gone
    auto report_after = GetTextureDebugReport(registry);
    EXPECT_EQ(report_after.tracked_assets.size(), 0) << "FreeUnusedAssets failed to clean 0-ref textures";
}
TEST_F(AssetManagerTest, MaterialToTextureDependency) {
    entt::entity tex = CreateTexture(registry, "tex_albedo", CreateDummyImage());
    entt::entity mat = CreateMaterial(registry, "mat_hero", CreateDummyMaterial());

    // Bind texture to material
    BindMaterialTexture(registry, mat, tex, MaterialTextureType::ALBEDO);

    // Check debug reports
    auto mat_report = GetMaterialDebugReport(registry);
    auto tex_report = GetTextureDebugReport(registry);

    // To check:
    // - The material holds 1 reference to the texture
    // - The material itself is not bound to any mesh yet, so it has 0 refs
    // - The debugger correctly mapped the link
    EXPECT_EQ(tex_report.tracked_assets["tex_albedo"].ref_count, 1);
    EXPECT_EQ(mat_report.tracked_assets["mat_hero"].ref_count, 0);
    ASSERT_EQ(mat_report.tracked_assets["mat_hero"].texture_links.size(), 1);
    EXPECT_EQ(mat_report.tracked_assets["mat_hero"].texture_links[0], "tex_albedo");

    // DESTROY MATERIAL -> Should cascade and destroy the texture
    DestroyAsset(registry, mat);
    EXPECT_EQ(GetMaterialDebugReport(registry).tracked_assets.size(), 0);
    EXPECT_EQ(GetTextureDebugReport(registry).tracked_assets.size(), 0) << "Texture survived even though its parent material was destroyed";
}
TEST_F(AssetManagerTest, MeshToMaterialDependency) {
    entt::entity mat1 = CreateMaterial(registry, "mat_red", CreateDummyMaterial());
    entt::entity mat2 = CreateMaterial(registry, "mat_blue", CreateDummyMaterial());
    entt::entity mesh = CreateMesh(registry, "mesh_car", CreateDummyMesh());

    // Bind materials to different parts of the mesh
    BindMaterial(registry, mesh, mat1, 0, 3);
    BindMaterial(registry, mesh, mat2, 3, 3);

    auto mat_report = GetMaterialDebugReport(registry);
    auto mesh_report = GetMeshDebugReport(registry);

    EXPECT_EQ(mat_report.tracked_assets["mat_red"].ref_count, 1);
    EXPECT_EQ(mat_report.tracked_assets["mat_blue"].ref_count, 1);
    ASSERT_EQ(mesh_report.tracked_assets["mesh_car"].material_links.size(), 2);
    
    // Destroy Mesh -> Cascades and destroys both materials
    DestroyAsset(registry, mesh);
    EXPECT_EQ(GetMeshDebugReport(registry).tracked_assets.size(), 0);
    EXPECT_EQ(GetMaterialDebugReport(registry).tracked_assets.size(), 0);
}
TEST_F(AssetManagerTest, BindMeshToWorldEntity) {
    entt::entity mesh = CreateMesh(registry, "mesh_box", CreateDummyMesh());
    
    entt::entity world_obj1 = registry.create();
    entt::entity world_obj2 = registry.create();

    // Two objects share the same mesh
    BindMesh(registry, world_obj1, mesh);
    BindMesh(registry, world_obj2, mesh);
    auto mesh_report = GetMeshDebugReport(registry);
    EXPECT_EQ(mesh_report.tracked_assets["mesh_box"].ref_count, 2);

    // Destroy obj1 world object
    // Mesh still exists because world_obj2 is keeping it alive
    registry.destroy(world_obj1);
    mesh_report = GetMeshDebugReport(registry);
    EXPECT_EQ(mesh_report.tracked_assets["mesh_box"].ref_count, 1);
    EXPECT_TRUE(registry.valid(mesh));

    // Destroy obj2 world object
    // Mesh does not exists -> No objects referencing it
    registry.destroy(world_obj2);
    mesh_report = GetMeshDebugReport(registry);
    EXPECT_EQ(mesh_report.tracked_assets["mesh_box"].ref_count, 0) << "mesh_box survived even though all its referencing objects were destroyed";
    EXPECT_TRUE(!registry.valid(mesh)) << "mesh_box entity is still alive even though all its referencing objects were destroyed";
}