// RRL/src/data/TextureSystem.cpp

#include "RRL/data/TextureComponents.hpp"
#include "RRL/rhi/RHIAPI.hpp"

#include "RRL/DebugMacros.hpp"
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


} // namespace rrl::data 

