// RRL/tests/asset_module_tests.cpp

#include "RRL/asset/MaterialAsset.hpp"
#include "RRL/asset/MeshAsset.hpp"
#include <gtest/gtest.h>
#include <memory>

#include <FLogging/FLogging.hpp>

// RRL Engine Modules
#include <RRL/RRLEngine.hpp>
#include <RRL/RRLTypes.hpp>

class AssetManagerTest : public ::testing::Test {
protected:
    std::unique_ptr<rrl::Engine> engine;

    void SetUp() override {
        flogging::AddConsoleSink();
        flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
        engine = std::make_unique<rrl::Engine>();
    }
    
    void TearDown() override {
        engine.reset();
        flogging::ResetLogger();
    }

    // Dummy generators 
    rrl::asset::ImageAsset CreateDummyImage() {
        rrl::asset::ImageAsset img;
        img.width           = 1;
        img.height          = 1;
        img.channels        = rrl::asset::ImageChannelLayout::CH_3;
        img.data_type       = rrl::asset::ImageAssetType::UINT8;
        img.color_layout    = rrl::asset::ImageColorLayout::RGB;
        img.data            = {255, 0, 0}; // 1 red pixel
        return img;
    }
    rrl::asset::MeshAsset CreateDummyMesh() {
        rrl::asset::MeshAsset mesh;
        mesh.topology = rrl::asset::MeshTopology::TRIANGLES;
        mesh.positions = {{0,0,0}, {1,0,0}, {0,1,0}};
        mesh.indices = {0, 1, 2};
        return mesh;
    }
    rrl::asset::MaterialAsset CreateDummyMaterial() {
        rrl::asset::MaterialAsset mat;
        return mat.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
    }
};



// --- Basic Asset API ---------------------------------------------
TEST_F(AssetManagerTest, CreateAndCacheTexture) {
    rrl::AssetID tex = engine->asset.CreateTexture("tex_diffuse", CreateDummyImage());
    
    // Assets texture report using the DebugModule
    auto report = engine->debug.GetTextureDebugReport();
    EXPECT_EQ(report.leaked_assets.size(), 0) << "Found leaked textures in the registry";
    EXPECT_EQ(report.dead_assets.size(), 0) << "Found dead string IDs in the cache";
    ASSERT_EQ(report.tracked_assets.size(), 1);

    // Verify Asset Stats
    auto& stats = report.tracked_assets["tex_diffuse"];
    EXPECT_EQ(stats.asset_id, tex);
    EXPECT_EQ(stats.ref_count, 0) << "New assets should start with 0 references";
}

TEST_F(AssetManagerTest, CascadeDeleteOnZeroRefs) {
    rrl::AssetID tex = engine->asset.CreateTexture("tex_temp", CreateDummyImage());
    
    // Simulate usage to increment the reference count
    rrl::ObjectID ui_obj = engine->scene.SpawnObject();
    engine->asset.BindUITexture(ui_obj, tex);
    EXPECT_EQ(engine->debug.GetTextureDebugReport().tracked_assets["tex_temp"].ref_count, 1);

    // Destroying the object should drop the ref count to 0, instantly triggering CASCADE_DELETE
    engine->scene.DestroyObject(ui_obj);
    
    auto report = engine->debug.GetTextureDebugReport();
    EXPECT_EQ(report.tracked_assets.size(), 0) << "Texture was not destroyed after ref count hit 0";
    EXPECT_EQ(report.leaked_assets.size(), 0);
    EXPECT_EQ(report.dead_assets.size(), 0);
}

TEST_F(AssetManagerTest, OverrideCachedAsset) {
    rrl::AssetID first_tex = engine->asset.CreateTexture("tex_shared", CreateDummyImage());
    
    // Override the ID with a new texture
    rrl::AssetID second_tex = engine->asset.CreateTexture("tex_shared", CreateDummyImage());

    EXPECT_NE(first_tex, second_tex);
    
    auto report = engine->debug.GetTextureDebugReport();
    EXPECT_EQ(report.tracked_assets.size(), 1) << "The first texture was not destroyed when overridden!";
    EXPECT_EQ(report.tracked_assets["tex_shared"].asset_id, second_tex);
    EXPECT_EQ(report.leaked_assets.size(), 0);
}


// --- Asset GB Dependencies ---------------------------------------
TEST_F(AssetManagerTest, KeepCachedAssetsPolicy) {
    engine->asset.SetAssetGCPolicy(rrl::asset::AssetGCPolicy::KEEP_CACHED_ASSETS);
    
    rrl::AssetID tex = engine->asset.CreateTexture("tex_keep", CreateDummyImage());
    
    // Simulate a brief usage spike
    rrl::ObjectID ui_obj = engine->scene.SpawnObject();
    engine->asset.BindUITexture(ui_obj, tex);
    engine->scene.DestroyObject(ui_obj);

    // It should STILL exist in the registry with 0 refs due to the policy
    auto report_before = engine->debug.GetTextureDebugReport();
    ASSERT_EQ(report_before.tracked_assets.size(), 1);
    EXPECT_EQ(report_before.tracked_assets["tex_keep"].ref_count, 0);

    // Manually flush unused assets
    engine->asset.FreeUnusedAssets();

    // Now it should be gone
    auto report_after = engine->debug.GetTextureDebugReport();
    EXPECT_EQ(report_after.tracked_assets.size(), 0) << "FreeUnusedAssets failed to clean 0-ref textures";
}

TEST_F(AssetManagerTest, MaterialToTextureDependency) {
    rrl::AssetID tex = engine->asset.CreateTexture("tex_albedo", CreateDummyImage());
    rrl::AssetID mat = engine->asset.CreateMaterial("mat_hero", CreateDummyMaterial());

    // Bind texture to material
    engine->asset.BindMaterialTexture(mat, tex, rrl::asset::MaterialTextureType::ALBEDO);

    // Check debug reports
    auto mat_report = engine->debug.GetMaterialDebugReport();
    auto tex_report = engine->debug.GetTextureDebugReport();

    // To check:
    // - The material holds 1 reference to the texture
    // - The material itself is not bound to any mesh yet, so it has 0 refs
    // - The debugger correctly mapped the link
    EXPECT_EQ(tex_report.tracked_assets["tex_albedo"].ref_count, 1);
    EXPECT_EQ(mat_report.tracked_assets["mat_hero"].ref_count, 0);
    ASSERT_EQ(mat_report.tracked_assets["mat_hero"].texture_links.size(), 1);
    EXPECT_EQ(mat_report.tracked_assets["mat_hero"].texture_links[0], "tex_albedo");

    // DESTROY MATERIAL -> Should cascade and destroy the texture
    engine->asset.DestroyAsset(mat);
    EXPECT_EQ(engine->debug.GetMaterialDebugReport().tracked_assets.size(), 0);
    EXPECT_EQ(engine->debug.GetTextureDebugReport().tracked_assets.size(), 0) << "Texture survived even though its parent material was destroyed";
}

TEST_F(AssetManagerTest, MeshToMaterialInstanceDependency) {
    // Create materials
    rrl::AssetID mat1 = engine->asset.CreateMaterial("mat_red", CreateDummyMaterial());
    rrl::AssetID mat2 = engine->asset.CreateMaterial("mat_blue", CreateDummyMaterial());
    
    // We modify the dummy mesh to explicitly have 2 submeshes. 
    // This ensures our smart BindMesh function strictly accepts both materials.
    rrl::asset::MeshAsset dummy_mesh = CreateDummyMesh();
    dummy_mesh.submeshes = {{0, 3}, {3, 3}}; 
    rrl::AssetID mesh_asset = engine->asset.CreateMesh("mesh_car", std::move(dummy_mesh));

    // bind the assets to a world object
    rrl::ObjectID world_object = engine->scene.SpawnObject();
    engine->asset.BindMesh(world_object, mesh_asset, {mat1, mat2});

    // Verify Reference Counts (Each asset has exactly 1 active user: the world_object)
    auto mat_report = engine->debug.GetMaterialDebugReport();
    auto mesh_report = engine->debug.GetMeshDebugReport();

    EXPECT_EQ(mat_report.tracked_assets["mat_red"].ref_count, 1);
    EXPECT_EQ(mat_report.tracked_assets["mat_blue"].ref_count, 1);
    EXPECT_EQ(mesh_report.tracked_assets["mesh_car"].ref_count, 1);
    
    // Unbind mat2
    // By passing only mat1, the binder will broadcast it to both submeshes.
    engine->asset.BindMesh(world_object, mesh_asset, {mat1});
    mat_report  = engine->debug.GetMaterialDebugReport();
    mesh_report = engine->debug.GetMeshDebugReport();
    
    EXPECT_TRUE(mat_report.tracked_assets.find("mat_blue") == mat_report.tracked_assets.end()) << "Mat_blue was not cleanly unlinked and destroyed";
    EXPECT_EQ(mat_report.tracked_assets["mat_red"].ref_count, 2);
    EXPECT_EQ(mesh_report.tracked_assets["mesh_car"].ref_count, 1);
    
    // Destroy the world object -> Triggers OnMeshLinkageDestroyed (cascade delete)
    engine->scene.DestroyObject(world_object);
    EXPECT_EQ(engine->debug.GetMeshDebugReport().tracked_assets.size(), 0);
    EXPECT_EQ(engine->debug.GetMaterialDebugReport().tracked_assets.size(), 0);
}

TEST_F(AssetManagerTest, BindMeshToWorldEntity) {
    rrl::AssetID mesh = engine->asset.CreateMesh("mesh_box", CreateDummyMesh());
    
    rrl::ObjectID world_obj1 = engine->scene.SpawnObject();
    rrl::ObjectID world_obj2 = engine->scene.SpawnObject();

    // Two objects share the same mesh
    engine->asset.BindMesh(world_obj1, mesh);
    engine->asset.BindMesh(world_obj2, mesh);
    auto mesh_report = engine->debug.GetMeshDebugReport();
    EXPECT_EQ(mesh_report.tracked_assets["mesh_box"].ref_count, 2);

    // Destroy obj1 world object
    // Mesh still exists because world_obj2 is keeping it alive
    engine->scene.DestroyObject(world_obj1);
    mesh_report = engine->debug.GetMeshDebugReport();
    EXPECT_EQ(mesh_report.tracked_assets["mesh_box"].ref_count, 1);

    // Destroy obj2 world object
    // Mesh does not exist -> No objects referencing it
    engine->scene.DestroyObject(world_obj2);
    mesh_report = engine->debug.GetMeshDebugReport();
    
    // Validate that mesh_box is not tracked anymore
    EXPECT_EQ(mesh_report.tracked_assets.size(), 0) << "mesh_box survived even though all its referencing objects were destroyed";
}