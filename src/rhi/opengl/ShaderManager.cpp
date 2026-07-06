#include "RRL/rhi/opengl/ShaderManager.hpp"

#include "RRL/data/MaterialData.hpp"
#include "RRL/rhi/opengl/EmbeddedShaders.hpp"

#include <FLogging/FLogging.hpp>
#include <filesystem>


namespace fs = std::filesystem;


namespace rrl::rhi::opengl {

data::ShadingModel ShaderManager::MapFilenameToShadingModel(const std::string& filename) {
    std::string lower_name = filename;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    // System shaders
    if (lower_name == "point_cloud") return data::ShadingModel::POINT_CLOUD;
    if (lower_name == "ui2d" || lower_name == "ui") return data::ShadingModel::UI2D;

    // Material shaders
    if (lower_name == "unlit")              return data::ShadingModel::UNLIT;
    if (lower_name == "phong")              return data::ShadingModel::PHONG;
    if (lower_name == "pbr")                return data::ShadingModel::PBR_OPAQUE;
    if (lower_name == "pbr_opaque")         return data::ShadingModel::PBR_OPAQUE;
    if (lower_name == "pbr_transparent")    return data::ShadingModel::PBR_TRANSPARENT;
    
    LOG_WARN("[ShaderManager] Unrecognized shader filename '{}'. Mapping to UNLIT by default.", filename);
    return data::ShadingModel::UNLIT;
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

            if (fs::exists(frag_path)) {
                auto shader = std::make_unique<Shader>();
                shader->LoadFromFile(vert_path, frag_path);
                
                if (shader->IsValid()) {
                    auto shading_model = MapFilenameToShadingModel(name);
                    loaded_shaders[shading_model] = std::move(shader);
                    LOG_INFO("[ShaderManager] Loaded shader program '{}' as shading model {}", name, static_cast<uint8_t>(shading_model));
                }
            } else {
                LOG_WARN("[ShaderManager] Found {}.vert but missing {}.frag", name, name);
            }
        }
    }

    // Fallback if the folder was empty
    if (loaded_shaders.find(data::ShadingModel::UNLIT) == loaded_shaders.end()) {
        LOG_WARN("[ShaderManager] 'unlit' shader missing from runtime folder. Injecting embedded fallback.");
        LoadShadersFromSource();
    }
}

void ShaderManager::LoadShadersFromSource() {
    LOG_INFO("[ShaderManager] Loading embedded shaders.");

    // Lambda to load a shader from source
    auto inject = [&](data::ShadingModel model, const char* vert, const char* frag) {
        if (loaded_shaders.find(model) == loaded_shaders.end()) {
            auto shader = std::make_unique<Shader>();
            shader->LoadFromSource(vert, frag);
            if (shader->IsValid()) {
                loaded_shaders[model] = std::move(shader);
            }
        }
    };

    inject(data::ShadingModel::UNLIT, shaders::unlit_vert, shaders::unlit_frag);
    // inject(data::ShadingModel::POINT_CLOUD, shaders::point_cloud_vert, shaders::point_cloud_frag);
    // inject(data::ShadingModel::UI2D, shaders::ui2d_vert, shaders::ui2d_frag);
}

Shader* ShaderManager::GetShader(data::ShadingModel shading_model) {
    auto it = loaded_shaders.find(shading_model);
    if (it != loaded_shaders.end()) {
        return it->second.get();
    }
    LOG_ERROR("[ShaderManager] Requested missing shading model: '{}'", static_cast<uint8_t>(shading_model) );
    return nullptr;
}

} // namespace rrl::rhi::opengl