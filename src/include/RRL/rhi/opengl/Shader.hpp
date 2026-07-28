// RRL/src/include/rhi/opengl/Shader.hpp
#pragma once

#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace rrl::rhi::opengl {


using ShaderID = uint32_t;
constexpr ShaderID SHADER_NULL = 0;


class Shader {
public:
    Shader() = default;
    ~Shader();

    // Rule of Five: Delete copying, allow moving
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    ShaderID GetID() const { return id; }
    bool IsValid() const { return id != SHADER_NULL; }

    // Load, compile and bind shaders to a GLProgram
    void LoadFromFile(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometric_path = "");
    void LoadFromSource(const std::string& vertex_source, const std::string& fragment_source, const std::string& geometric_source = "");
    
    void Use() const;

    // Uniform setters
    void SetMat4(const std::string& name, const glm::mat4& mat);
    void SetVec3(const std::string& name, const glm::vec3& vec);
    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);

private:
    void MapProgramUniformNames();
    int GetUniformLocation(const std::string& name);

    ShaderID id = SHADER_NULL;
    std::unordered_map<std::string, int> uniform_locations; // Cache locations 
};

} // namespace rrl::rhi::opengl