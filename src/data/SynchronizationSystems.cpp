// RRL/src/data/TextureSystem.cpp

#include "RRL/data/SynchronizationSystems.hpp"

#include "RRL/data/TextureComponents.hpp"
#include "RRL/data/MeshComponents.hpp"
#include "RRL/data/MaterialData.hpp"
#include "RRL/data/MaterialComponents.hpp"

#include "RRL/rhi/RHIAPI.hpp"

#include <cstdint>


namespace rrl::data {

void SyncTexturesToRHI(entt::registry& registry) {
    auto view = registry.view<TextureSourceComponent>();
    for (auto entity: view) {
        auto& source = view.get<TextureSourceComponent>(entity);
        uint32_t current_version = source.version.load(std::memory_order_acquire);
        
        if (!registry.all_of<TextureRuntimeComponent>(entity)) {
            registry.emplace<TextureRuntimeComponent>(entity);
        }
        auto& runtime = registry.get<TextureRuntimeComponent>(entity);
        

        if ( runtime.cached_tex_version != current_version && source.image ) {

            // Create a new texture
            if (runtime.handle == rhi::TEXTURE_NULL) {
                runtime.handle = rhi::CreateTexture(registry, *source.image);
            }
            // Update the existing texture
            else {
                rhi::UpdateTexture(registry, runtime.handle, *source.image);
            }
            runtime.cached_tex_version = current_version;
        }
        
    }
    
}
void SyncMeshesToRHI(entt::registry& registry) {
    auto view = registry.view<MeshSourceComponent>();
    for (auto entity: view) {
        auto& source = view.get<MeshSourceComponent>(entity);
        uint32_t current_version = source.version.load(std::memory_order_acquire);
        
        if (!registry.all_of<MeshRuntimeComponent>(entity)) {
            registry.emplace<MeshRuntimeComponent>(entity);
        }
        auto& runtime = registry.get<MeshRuntimeComponent>(entity);
        
        if ( runtime.cached_mesh_version != current_version && source.mesh ) {

            // Create a new mesh
            if (runtime.handle == rhi::MESH_NULL) {
                runtime.handle = rhi::CreateMesh(registry, *source.mesh);
            }
            // Update the existing mesh
            else {
                rhi::UpdateMesh(registry, runtime.handle, *source.mesh);
            }
            runtime.cached_mesh_version = current_version;
        }
        
    }
}
void SyncMaterialsToRHI(entt::registry& registry) {
    auto view = registry.view<MaterialData>(entt::exclude<MaterialRuntimeComponent>);
    
    for (auto entity : view) {
        const auto& mat_data = view.get<MaterialData>(entity);
        auto& runtime = registry.emplace<MaterialRuntimeComponent>(entity);
        
        runtime.handle = rhi::CreateMaterial(registry, mat_data);
        
        // Resolve Albedo Map
        if (registry.valid(mat_data.albedo_map) && registry.all_of<TextureRuntimeComponent>(mat_data.albedo_map)) {
            runtime.albedo_handle = registry.get<TextureRuntimeComponent>(mat_data.albedo_map).handle;
        } else {
            runtime.albedo_handle = rhi::TEXTURE_NULL;
        }

        // Resolve Normal Map
        if (registry.valid(mat_data.normal_map) && registry.all_of<TextureRuntimeComponent>(mat_data.normal_map)) {
            runtime.normal_handle = registry.get<TextureRuntimeComponent>(mat_data.normal_map).handle;
        } else {
            runtime.normal_handle = rhi::TEXTURE_NULL;
        }

        // Resolve Metallic/Roughness Map
        if (registry.valid(mat_data.metallic_roughness_map) && registry.all_of<TextureRuntimeComponent>(mat_data.metallic_roughness_map)) {
            runtime.metallic_roughness_handle = registry.get<TextureRuntimeComponent>(mat_data.metallic_roughness_map).handle;
        } else {
            runtime.metallic_roughness_handle = rhi::TEXTURE_NULL;
        }
    }
}


} // namespace rrl::data 

