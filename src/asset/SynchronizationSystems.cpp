// RRL/src/data/TextureSystem.cpp

#include "RRL/asset/SynchronizationSystems.hpp"

#include "RRL/EnttCasting.hpp"
#include "RRL/asset/TextureComponents.hpp"
#include "RRL/asset/MeshComponents.hpp"
#include "RRL/asset/MaterialAsset.hpp"
#include "RRL/asset/MaterialComponents.hpp"

#include "RRL/rhi/RHI.hpp"

#include <cstdint>


namespace rrl::asset {

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
    auto view = registry.view<MaterialSourceComponent>();
    
    for (auto entity : view) {
        auto& source = view.get<MaterialSourceComponent>(entity);
        uint32_t current_version = source.version.load(std::memory_order_acquire);
        
        if (!registry.all_of<MaterialRuntimeComponent>(entity)) {
            registry.emplace<MaterialRuntimeComponent>(entity);
        }
        auto& runtime = registry.get<MaterialRuntimeComponent>(entity);
        
        // If the material properties or assigned textures changed
        if (runtime.cached_mat_version != current_version) {
            
            if (runtime.handle == rhi::MATERIAL_NULL) {
                runtime.handle = rhi::CreateMaterial(registry, source.data);
            } else {
                rhi::UpdateMaterial(registry, runtime.handle, source.data);
            }
            
            // Resolve Albedo Map
            if (registry.valid( ToEntt(source.data.albedo_map) ) && registry.all_of<TextureRuntimeComponent>( ToEntt(source.data.albedo_map) )) {
                runtime.albedo_handle = registry.get<TextureRuntimeComponent>( ToEntt(source.data.albedo_map) ).handle;
            } else {
                runtime.albedo_handle = rhi::TEXTURE_NULL;
            }

            // Resolve Normal Map
            if (registry.valid( ToEntt(source.data.normal_map) ) && registry.all_of<TextureRuntimeComponent>( ToEntt(source.data.normal_map) )) {
                runtime.normal_handle = registry.get<TextureRuntimeComponent>( ToEntt(source.data.normal_map) ).handle;
            } else {
                runtime.normal_handle = rhi::TEXTURE_NULL;
            }

            // Resolve Metallic/Roughness Map
            if (registry.valid( ToEntt(source.data.metallic_roughness_map) ) && registry.all_of<TextureRuntimeComponent>( ToEntt(source.data.metallic_roughness_map) )) {
                runtime.metallic_roughness_handle = registry.get<TextureRuntimeComponent>( ToEntt(source.data.metallic_roughness_map) ).handle;
            } else {
                runtime.metallic_roughness_handle = rhi::TEXTURE_NULL;
            }

            // Resolve Emmisive Map
            if (registry.valid( ToEntt(source.data.emissive_map) ) && registry.all_of<TextureRuntimeComponent>( ToEntt(source.data.emissive_map) )) {
                runtime.metallic_roughness_handle = registry.get<TextureRuntimeComponent>( ToEntt(source.data.emissive_map) ).handle;
            } else {
                runtime.metallic_roughness_handle = rhi::TEXTURE_NULL;
            }

            runtime.cached_mat_version = current_version;
        }
    }
}


} // namespace rrl::asset 

