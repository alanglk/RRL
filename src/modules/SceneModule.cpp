// RRL/src/modules/SceneModule.cpp

#include "RRL/modules/SceneModule.hpp"
#include "RRL/scene/PrefabManager.hpp"

#include "RRL/EngineContext.hpp"
#include "RRL/EnttCasting.hpp"
#include "RRL/scene/SceneTypes.hpp"


namespace rrl {


// --- Constructor / Destructor ------------------------------------
SceneModule::SceneModule(EngineContext* ctx): m_ctx(ctx) {
    // Automatically boot up the Scene Manager's internal caches and hooks
    rrl::scene::InitializePrefabManager(m_ctx->registry);
}
SceneModule::~SceneModule() { }


// --- Basic Node Lifecycle ----------------------------------------
ObjectID SceneModule::SpawnObject() {
    // Simply ask EnTT for a fresh ID
    return ToObjectID(m_ctx->registry.create());
}
void SceneModule::DestroyObject(ObjectID object) {
    entt::entity ent = ToEntt(object);
    if (m_ctx->registry.valid(ent)) {
        m_ctx->registry.destroy(ent);
    }
}


// --- Prefabs -----------------------------------------------------
void SceneModule::PreloadPrefabBlueprint(const scene::PrefabID& blueprint_id, rrl::io::IOPrefab&& prefab_data) {
    rrl::scene::PreloadPrefabBlueprint(m_ctx->registry, blueprint_id, std::move(prefab_data));
}
ObjectID SceneModule::SpawnPrefab(const scene::PrefabID& blueprint_id) {
    entt::entity root_entity = rrl::scene::SpawnPrefab(m_ctx->registry, blueprint_id);
    return ToObjectID(root_entity);
}
void SceneModule::DestroyPrefab(ObjectID prefab_object, bool force_asset_deletion) {
    rrl::scene::DestroyPrefab(m_ctx->registry, ToEntt(prefab_object), force_asset_deletion);
}


} // namespace rrl