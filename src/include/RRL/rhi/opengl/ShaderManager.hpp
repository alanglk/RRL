// RRL/src/include/rhi/opengl/ShaderManager.hpp
#pragma once
#include "RRL/rhi/RHITypes.hpp"
#include "RRL/rhi/opengl/Shader.hpp"
#include "RRL/asset/MaterialAsset.hpp"


#include <unordered_map>
#include <memory>
#include <string>

namespace rrl::rhi::opengl {

class ShaderManager {

    // Helper to map a shader file name into the correcponding statically defined shading model
    static rrl::asset::ShadingModel MapFilenameToShadingModel(const std::string& filename);
public:

    static ShaderManager& I() {
        static ShaderManager instance;
        return instance;
    }

    // Delete copy/move
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    void LoadShadersFromDirectory(const std::string& directory_path);
    void LoadShadersFromSource(); // Loads the embedded fallback shaders
    
    // Returns a raw pointer to the material shader
    Shader* GetMaterialShader(rrl::asset::ShadingModel model);

    // Returns a raw pointer to the pipeline shader
    Shader* GetSystemShader(rrl::rhi::RHIBuiltinRenderPipeline pipeline);
    
    // Clean shaders
    void ClearShaders();

private:
    ShaderManager() = default;
    ~ShaderManager() = default;

    // The manager owns the actual OpenGL programs
    std::unordered_map<rrl::asset::ShadingModel, std::unique_ptr<Shader>> m_material_shaders;
    std::unordered_map<rrl::rhi::RHIBuiltinRenderPipeline, std::unique_ptr<Shader>> m_system_shaders;
};


} // namespace rrl::rhi::opengl