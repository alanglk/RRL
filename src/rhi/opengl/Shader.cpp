// RRL/src/rhi/opengl/Shader.cpp
#include "RRL/rhi/opengl/Shader.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
namespace fs = std::filesystem;

#include <FLogging/FLogging.hpp>


namespace rrl::rhi::opengl {

    
Shader::~Shader() {
    if (id != SHADER_NULL) {
        glDeleteProgram(id);
        id = SHADER_NULL;
    }
}
Shader::Shader(Shader&& other) noexcept : id(other.id), uniform_locations(std::move(other.uniform_locations)) {
    other.id = SHADER_NULL;
}
Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (id != SHADER_NULL) glDeleteProgram(id);
        id = other.id;
        uniform_locations = std::move(other.uniform_locations);
        other.id = SHADER_NULL;
    }
    return *this;
}

void Shader::LoadFromFile(const std::string& vertex_path, const std::string& fragment_path, const std::string& geometric_path) {
    auto read_file = [](const std::string& path, const std::vector<std::string>& valid_exts) -> std::string {
        if (path.empty()) return "";
        fs::path p(path);
        
        if (!fs::exists(p) || !fs::is_regular_file(p)) {
            LOG_ERROR("Shader file not found: {}", path);
            return "";
        }
        
        std::string ext = p.extension().string();
        bool valid = false;
        for (const auto& ve : valid_exts) { if (ext == ve) valid = true; }
        
        if (!valid) {
            LOG_ERROR("Invalid shader extension for file: {}", path);
            return "";
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open shader file: {}", path);
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    };

    std::string vtx_data = read_file(vertex_path, {".vert", ".vtx", ".vs"});
    std::string frag_data = read_file(fragment_path, {".frag", ".fragment", ".fs"});
    std::string geom_data = read_file(geometric_path, {".geom", ".gs"});

    if (vtx_data.empty() || frag_data.empty()) return;
    
    LoadFromSource(vtx_data, frag_data, geom_data);
}
void Shader::LoadFromSource(const std::string& vertex_source, const std::string& fragment_source, const std::string& geometric_source) {
    if (vertex_source.empty() || fragment_source.empty()) {
        LOG_ERROR("Shader::LoadFromSource requires both vertex and fragment source.");
        return;
    }

    // Lambda for compiling a shader source code
    auto compile_shader = [](const char* source, GLenum type, const char* name) -> GLuint {
        GLuint shader_id = glCreateShader(type);
        glShaderSource(shader_id, 1, &source, nullptr);
        glCompileShader(shader_id);
        
        GLint success;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info_log[512];
            glGetShaderInfoLog(shader_id, 512, nullptr, info_log);
            LOG_ERROR("Error compiling {}: {}", name, info_log);
            glDeleteShader(shader_id);
            return SHADER_NULL;
        }
        return shader_id;
    };
    
    GLuint vtx_id  = compile_shader(vertex_source.c_str(), GL_VERTEX_SHADER, "Vertex Shader");
    GLuint frag_id = compile_shader(fragment_source.c_str(), GL_FRAGMENT_SHADER, "Fragment Shader");
    GLuint geom_id = SHADER_NULL;
    
    if (!geometric_source.empty()) {
        geom_id = compile_shader(geometric_source.c_str(), GL_GEOMETRY_SHADER, "Geometry Shader");
    }
    if (vtx_id == SHADER_NULL || frag_id == SHADER_NULL) return;
    
    // Link shaders
    id = glCreateProgram();
    glAttachShader(id, vtx_id);
    glAttachShader(id, frag_id);
    if (geom_id != SHADER_NULL) glAttachShader(id, geom_id);
    glLinkProgram(id);

    // check for linking errors
    GLint success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(id, 512, nullptr, info_log);
        LOG_ERROR("Error linking shader program: {}", info_log);
        glDeleteProgram(id);
        id = SHADER_NULL;
    }
    
    // Free already linked shader objects
    glDeleteShader(vtx_id);
    glDeleteShader(frag_id);
    if (geom_id != SHADER_NULL) glDeleteShader(geom_id);

    // Map uniform names and locations
    if (id != SHADER_NULL) {
        MapProgramUniformNames();
    }
}
void Shader::Use() const {
    if (id != SHADER_NULL) {
        glUseProgram(id);
    }
}
void Shader::MapProgramUniformNames() {
    uniform_locations.clear();
    if (id == SHADER_NULL) return;
    
    GLint num_uniforms = 0;
    GLint max_name_len = 0;
    glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &num_uniforms);
    glGetProgramiv(id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_len);
    
    std::vector<GLchar> name_data(max_name_len);
    for (int i = 0; i < num_uniforms; i++) {
        GLint size;
        GLenum type;
        GLsizei len;
        
        glGetActiveUniform(id, i, name_data.size(), &len, &size, &type, &name_data[0]);
        std::string name(&name_data[0], len);
        
        // Cache integer location for fast lookups during RenderFrame
        uniform_locations[name] = glGetUniformLocation(id, name.c_str());
    }
}
int Shader::GetUniformLocation(const std::string& name) {
    auto it = uniform_locations.find(name);
    if (it != uniform_locations.end()) {
        return it->second;
    }
    LOG_WARN("Shader Uniform '{}' not found or not actively used in shader code.", name);
    return -1;
}

void Shader::SetMat4(const std::string& name, const glm::mat4& mat) {
    // OpenGL 4.5 DSA (glProgramUniform instead of glUniform)
    int loc = GetUniformLocation(name);
    if (loc != -1) glProgramUniformMatrix4fv(id, loc, 1, GL_FALSE, glm::value_ptr(mat));
}
void Shader::SetVec3(const std::string& name, const glm::vec3& vec) {
    int loc = GetUniformLocation(name);
    if (loc != -1) {
        glProgramUniform3fv(id, loc, 1, glm::value_ptr(vec));
    }
}
void Shader::SetInt(const std::string& name, int value) {
    int loc = GetUniformLocation(name);
    if (loc != -1) {
        glProgramUniform1i(id, loc, value);
    }
}
void Shader::SetFloat(const std::string& name, float value) {
    int loc = GetUniformLocation(name);
    if (loc != -1) {
        glProgramUniform1f(id, loc, value);
    }
}

} // namespace rrl::rhi::opengl