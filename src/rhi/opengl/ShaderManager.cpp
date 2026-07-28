#include "RRL/rhi/opengl/ShaderManager.hpp"

#include "RRL/asset/MaterialAsset.hpp"
#include "RRL/rhi/RHITypes.hpp"
#include "RRL/rhi/opengl/EmbeddedShaders.hpp"

#include <FLogging/FLogging.hpp>
#include <filesystem>
#include <optional>


namespace fs = std::filesystem;


namespace rrl::rhi::opengl {

static std::optional<rrl::asset::ShadingModel> MapFilenameToMatShadingModel(const std::string& lower_name) {
    if (lower_name == "mat_unlit")              return rrl::asset::ShadingModel::UNLIT;
    if (lower_name == "mat_phong")              return rrl::asset::ShadingModel::PHONG;
    if (lower_name == "mat_pbr_opaque")         return rrl::asset::ShadingModel::PBR_OPAQUE;
    if (lower_name == "mat_pbr_transparent")    return rrl::asset::ShadingModel::PBR_TRANSPARENT;
    
    return std::nullopt;
}

static std::optional<rrl::rhi::RHIBuiltinRenderPipeline> MapFilenameToSysPipeline(const std::string& lower_name) {
    if (lower_name == "sys_point_cloud")        return rrl::rhi::RHIBuiltinRenderPipeline::POINT_CLOUD;
    if (lower_name == "sys_ui2d")               return rrl::rhi::RHIBuiltinRenderPipeline::UI2D;
    
    return std::nullopt;
}

void ShaderManager::LoadShadersFromDirectory(const std::string& directory_path) {
    if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
        LOG_WARN("[ShaderManager] Runtime shader directory not found: {}. Falling back to embedded shaders.", directory_path);
        LoadShadersFromSource();
        return;
    }

    LOG_INFO("[ShaderManager] Loading runtime shaders from: {}", directory_path);
    
    // Iterate over the directory looking for vertex shaders as the anchor
    for (const auto& entry : fs::directory_iterator(directory_path)) {
        if (entry.path().extension() == ".vert") {
            std::string name = entry.path().stem().string();
            std::string vert_path = entry.path().string();
            std::string frag_path = (entry.path().parent_path() / (name + ".frag")).string();

            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            
            
            if (fs::exists(frag_path)) {
                auto shader = std::make_unique<Shader>();
                shader->LoadFromFile(vert_path, frag_path);
                
                if (shader->IsValid()) {
                    
                    // Check if it's a Material Shader
                    if (auto mat_model = MapFilenameToMatShadingModel(lower_name)) {
                        m_material_shaders[*mat_model] = std::move(shader);
                        LOG_INFO("[ShaderManager] Loaded material shader '{}' as shading model {}", name, static_cast<uint8_t>(*mat_model));
                    } 
                    // Check if it's a System Pipeline Shader
                    else if (auto sys_pipe = MapFilenameToSysPipeline(lower_name)) {
                        m_system_shaders[*sys_pipe] = std::move(shader);
                        LOG_INFO("[ShaderManager] Loaded systemic shader '{}' as built-in pipeline {}", name, static_cast<uint8_t>(*sys_pipe));
                    } 
                    // Unknown
                    else {
                        LOG_WARN("[ShaderManager] Unrecognized shader prefix/name '{}'. Ignoring.", name);
                    }

                }
            } else {
                LOG_WARN("[ShaderManager] Found {}.vert but missing {}.frag", name, name);
            }

        }
    }

    // Fallback on non found shaders
    LoadShadersFromSource();
}

void ShaderManager::LoadShadersFromSource() {
    LOG_INFO("[ShaderManager] Loading embedded shaders.");

    // Lambda to load a material shader from source
    auto inject_mat = [&](rrl::asset::ShadingModel model, const char* vert, const char* frag) {
        if (m_material_shaders.find(model) == m_material_shaders.end()) {
            auto shader = std::make_unique<Shader>();
            shader->LoadFromSource(vert, frag);
            if (shader->IsValid()) {
                m_material_shaders[model] = std::move(shader);
            }
        }
    };

    // Lambda to load a systemic pipeline shader from source
    auto inject_sys = [&](rrl::rhi::RHIBuiltinRenderPipeline pipeline, const char* vert, const char* frag) {
        if (m_system_shaders.find(pipeline) == m_system_shaders.end()) {
            auto shader = std::make_unique<Shader>();
            shader->LoadFromSource(vert, frag);
            if (shader->IsValid()) {
                m_system_shaders[pipeline] = std::move(shader);
            }
        }
    };

    inject_mat(rrl::asset::ShadingModel::UNLIT, shaders::unlit_vert, shaders::unlit_frag);
    inject_sys(rrl::rhi::RHIBuiltinRenderPipeline::POINT_CLOUD, shaders::point_cloud_vert, shaders::point_cloud_frag);
    inject_sys(rrl::rhi::RHIBuiltinRenderPipeline::UI2D, shaders::ui2d_vert, shaders::ui2d_frag);
}

Shader* ShaderManager::GetMaterialShader(rrl::asset::ShadingModel shading_model) {
    auto it = m_material_shaders.find(shading_model);
    if (it != m_material_shaders.end()) {
        return it->second.get();
    }
    LOG_ERROR("[ShaderManager] Requested missing material shading model: '{}'", static_cast<uint8_t>(shading_model));
    return nullptr;
}
Shader* ShaderManager::GetSystemShader(rrl::rhi::RHIBuiltinRenderPipeline pipeline) {
    auto it = m_system_shaders.find(pipeline);
    if (it != m_system_shaders.end()) {
        return it->second.get();
    }
    LOG_ERROR("[ShaderManager] Requested missing systemic pipeline shader: '{}'", static_cast<uint8_t>(pipeline));
    return nullptr;
}
void ShaderManager::ClearShaders() {
    m_material_shaders.clear();
    m_system_shaders.clear();
}


} // namespace rrl::rhi::opengl