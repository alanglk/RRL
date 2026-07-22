// RRL/src/scene/SceneManager.cpp

#include "RRL/scene/SceneManager.hpp"
#include "RRL/scene/SceneCache.hpp"

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


namespace rrl::scene {


// --- Lifecycle ---------------------------------------------------
void InitializeSceneManager(entt::registry& registry) {
    registry.ctx().emplace<scene::SceneCache>();
}
entt::entity SpawnObject(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<SceneCache>();
    
    // Track it in the cache
    entt::entity new_ent = registry.create();
    cache.active_objects.insert(new_ent);
    return new_ent;
}
void DestroyObject(entt::registry& registry, entt::entity object) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<SceneCache>();
    
    // Remove it from the cache. Garbage collection is ensured by EnTT component destructors.
    if (registry.valid(object)) {
        cache.active_objects.erase(object);
        registry.destroy(object);
    }
}
void DestroyAllObjects(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<SceneCache>();
    
    // Nuke all tracked entities
    for (entt::entity ent : cache.active_objects) {
        if (registry.valid(ent)) {
            registry.destroy(ent);
        }
    }
    
    // Reset the tracker
    cache.active_objects.clear();
}


// --- Environment -------------------------------------------------
void SetEnvironmentColor(entt::registry& registry, const glm::vec4& color) {
    RRL_ASSERT(registry.ctx().contains<rrl::scene::SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<scene::SceneCache>();

    cache.environment.type = scene::SceneEnvironmentType::SOLID_COLOR;
    cache.environment.clear_color = color;
}
void SetEnvironmentCubemap(entt::registry& registry, rrl::AssetID cubemap_asset) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<SceneCache>();
    
    cache.environment.type = SceneEnvironmentType::SKYBOX_CUBEMAP;
    cache.environment.environment_texture = cubemap_asset;
}
void SetEnvironmentEquirectangular(entt::registry& registry, rrl::AssetID texture_asset) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<SceneCache>();
    
    cache.environment.type = SceneEnvironmentType::SKYBOX_EQUIRECTANGULAR;
    cache.environment.environment_texture = texture_asset;
}
void SetEnvironmentCustomMesh(entt::registry& registry, rrl::AssetID texture_asset, rrl::AssetID mesh_asset) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    auto& cache = registry.ctx().get<SceneCache>();
    
    cache.environment.type = SceneEnvironmentType::CUSTOM_MESH;
    cache.environment.environment_texture = texture_asset;
    cache.environment.custom_mesh = mesh_asset;
}
const SceneEnvironment& GetEnvironment(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    return registry.ctx().get<SceneCache>().environment;
}





} // namespace rrl::scene

