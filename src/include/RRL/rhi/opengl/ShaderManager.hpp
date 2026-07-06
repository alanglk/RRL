// RRL/src/include/rhi/opengl/ShaderManager.hpp
#pragma once
#include "RRL/rhi/opengl/Shader.hpp"
#include "RRL/data/MaterialData.hpp"


#include <unordered_map>
#include <memory>
#include <string>

namespace rrl::rhi::opengl {

class ShaderManager {

    // Helper to map a shader file name into the correcponding statically defined shading model
    static data::ShadingModel MapFilenameToShadingModel(const std::string& filename);
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
    
    // Returns a raw pointer to the shader
    Shader* GetShader(data::ShadingModel shading_model);

private:
    ShaderManager() = default;
    ~ShaderManager() = default;

    // The manager owns the actual OpenGL programs
    std::unordered_map<data::ShadingModel, std::unique_ptr<Shader>> loaded_shaders;
};


} // namespace rrl::rhi::opengl