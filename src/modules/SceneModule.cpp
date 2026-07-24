// RRL/src/modules/SceneModule.cpp

#include "RRL/modules/SceneModule.hpp"
#include "RRL/scene/PrefabInstanceManager.hpp"

#include "RRL/EngineContext.hpp"
#include "RRL/EnttCasting.hpp"

#include "RRL/scene/SceneManager.hpp"


namespace rrl {


// --- Constructor / Destructor ------------------------------------
SceneModule::SceneModule(EngineContext* ctx): m_ctx(ctx) {
    rrl::scene::InitializeSceneManager(m_ctx->registry);
}
SceneModule::~SceneModule() {}


// --- Basic Node Lifecycle ----------------------------------------
ObjectID SceneModule::SpawnObject() {
    // Simply ask EnTT for a fresh ID
    return ToObjectID(rrl::scene::SpawnObject(m_ctx->registry));
}
void SceneModule::DestroyObject(ObjectID object) {
    rrl::scene::DestroyObject(m_ctx->registry, ToEntt(object));
}


// --- Prefabs -----------------------------------------------------
ObjectID SceneModule::SpawnPrefab(const rrl::asset::PrefabID& blueprint_id) {
    entt::entity root_entity = rrl::scene::SpawnPrefabInstance(m_ctx->registry, blueprint_id);
    return ToObjectID(root_entity);
}
void SceneModule::DestroyPrefab(ObjectID prefab_object, bool force_asset_deletion) {
    rrl::scene::DestroyPrefabInstance(m_ctx->registry, ToEntt(prefab_object), force_asset_deletion);
}


// --- Scene Environment -------------------------------------------
void SceneModule::SetEnvironmentColor(const glm::vec4& color) {
    rrl::scene::SetEnvironmentColor(m_ctx->registry, color);
}
void SceneModule::SetEnvironmentEquirectangular(rrl::AssetID texture_asset) {
    rrl::scene::SetEnvironmentEquirectangular(m_ctx->registry, texture_asset);
}
void SceneModule::SetEnvironmentCubemap(rrl::AssetID cubemap_asset) {
    rrl::scene::SetEnvironmentCubemap(m_ctx->registry, cubemap_asset);
}
void SceneModule::SetEnvironmentCustomMesh(rrl::AssetID texture_asset, rrl::AssetID mesh_asset) {
    rrl::scene::SetEnvironmentCustomMesh(m_ctx->registry, texture_asset, mesh_asset);
}



} // namespace rrl