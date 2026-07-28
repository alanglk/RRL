// src/scene/SceneSynchronizationSystems.hpp

#include "RRL/scene/SceneSynchronizationSystems.hpp"
#include "RRL/EnttCasting.hpp"
#include "RRL/asset/MeshComponents.hpp"
#include "RRL/asset/TextureComponents.hpp"
#include "RRL/scene/SceneCache.hpp"

#include "RRL/rhi/RHI.hpp"


#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


namespace rrl::scene {


void SyncSceneToRHI(entt::registry& registry) {
    RRL_ASSERT(registry.ctx().contains<SceneCache>(), "SceneManager not initialized!");
    if (!registry.ctx().contains<rrl::scene::SceneRuntimeCache>()) {
        registry.ctx().emplace<rrl::scene::SceneRuntimeCache>();
    }
    auto& scene_cache = registry.ctx().get<rrl::scene::SceneCache>();
    auto& runtime_cache = registry.ctx().get<SceneRuntimeCache>();


    // Environment Synchronization
    uint32_t current_env_version = scene_cache.environment_version.load(std::memory_order_acquire);
    if (runtime_cache.cached_env_version != current_env_version) {
        
        rhi::PhysicalEnvironmentDescriptor phys_env;
        phys_env.type = scene_cache.environment.type;
        phys_env.clear_color = scene_cache.environment.clear_color;
        
        // Resolve Physical Texture Handle
        if (scene_cache.environment.environment_texture != rrl::NULL_ASSET) {
            entt::entity tex_ent = ToEntt(scene_cache.environment.environment_texture);
            if (registry.valid(tex_ent) && registry.all_of<rrl::asset::TextureRuntimeComponent>(tex_ent)) {
                phys_env.environment_texture = registry.get<rrl::asset::TextureRuntimeComponent>(tex_ent).handle;
            }
        }
        
        // Resolve Physical Mesh Handle
        if (scene_cache.environment.custom_mesh != rrl::NULL_ASSET) {
            entt::entity mesh_ent = ToEntt(scene_cache.environment.custom_mesh);
            if (registry.valid(mesh_ent) && registry.all_of<rrl::asset::MeshRuntimeComponent>(mesh_ent)) {
                phys_env.custom_mesh = registry.get<rrl::asset::MeshRuntimeComponent>(mesh_ent).handle;
            }
        }

        // Dispatch to RHI
        rrl::rhi::SetEnvironment(registry, phys_env);
        
        // Mark as synced
        runtime_cache.cached_env_version = current_env_version;
    }



    // Other scene elements Synchronization
}


}
