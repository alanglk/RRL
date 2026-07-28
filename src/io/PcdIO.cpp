// RRL/src/io/PcdIO.cpp

#include "RRL/io/PcdIO.hpp"

#include <fstream>
#include <sstream>
#include <glm/glm.hpp>

#include <FLogging/FLogging.hpp>


namespace rrl::io {
    
// --- Helpers -----------------------------------------------------
// Parse string to PCDFieldName 
static PCDFieldName ParseFieldName(const std::string& name) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name == "x") return PCDFieldName::X;
    if (lower_name == "y") return PCDFieldName::Y;
    if (lower_name == "z") return PCDFieldName::Z;
    if (lower_name == "rgb" || lower_name == "rgba") return PCDFieldName::RGB;
    if (lower_name == "intensity" || lower_name == "i") return PCDFieldName::INTENSITY;
    
    return PCDFieldName::UNKNOWN;
}
// Parse string to PCDDimensionType 
static PCDDimensionType ParseDimensionType(char t) {
    if (t == 'I' || t == 'i') return PCDDimensionType::I;
    if (t == 'U' || t == 'u') return PCDDimensionType::U;
    return PCDDimensionType::F; // Default to float
}
static PCDDataType ParseDataType(const std::string& type_str) {
    std::string lower_type = type_str;
    std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);
    
    if (lower_type == "ascii")  return PCDDataType::ASCII;
    if (lower_type == "binary") return PCDDataType::BINARY;
    if (lower_type == "binary_compressed") return PCDDataType::BINARY_COMPRESSED;

    LOG_ERROR("[PcdIO] Unkown PCDDataType. Defaulting to ASCII pcd type");
    return PCDDataType::ASCII; 
}
// Transform a uin32_t hex color into an unpacket float3 vector
static glm::vec4 UnpackPCDColor(uint32_t rgb_packed) {
    float r = ((rgb_packed >> 16) & 0xFF) / 255.0f;
    float g = ((rgb_packed >> 8)  & 0xFF) / 255.0f;
    float b = (rgb_packed         & 0xFF) / 255.0f;
    return glm::vec4(r, g, b, 1.0f);
}
// Read point cloud stored intensity into a float type
static float NormalizeIntensity(float raw_val, PCDDimensionType type, int size) {
    // If it's stored as an integer, it usually maps to 0-255 or max bit range. 
    // If it's a float, it's often already 0.0-1.0, but occasionally 0.0-255.0.
    if (type == PCDDimensionType::U && size == 1) return raw_val / 255.0f;
    if (type == PCDDimensionType::U && size == 2) return raw_val / 65535.0f;
    if (type == PCDDimensionType::F && raw_val > 1.0f) return raw_val / 255.0f;
    return raw_val; // Fallback
}
// Binary data reader helper
template <typename T>
static float ReadBinaryField(const char* data) {
    return static_cast<float>(*reinterpret_cast<const T*>(data));
}
static float ExtractBinaryValue(const char* data, PCDDimensionType type, int size) {
    if (type == PCDDimensionType::F) {
        if (size == 4) return ReadBinaryField<float>(data);
        if (size == 8) return ReadBinaryField<double>(data);
    } else if (type == PCDDimensionType::U) {
        if (size == 1) return ReadBinaryField<uint8_t>(data);
        if (size == 2) return ReadBinaryField<uint16_t>(data);
        if (size == 4) return ReadBinaryField<uint32_t>(data); 
    } else if (type == PCDDimensionType::I) {
        if (size == 1) return ReadBinaryField<int8_t>(data);
        if (size == 2) return ReadBinaryField<int16_t>(data);
        if (size == 4) return ReadBinaryField<int32_t>(data);
    }
    return 0.0f;
}


// --- Implementation ----------------------------------------------
asset::MeshAsset LoadPCDFile(const std::string& pcd_path) {
    asset::MeshAsset mesh;
    mesh.topology = asset::MeshTopology::POINTS;

    std::ifstream file(pcd_path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("[PcdIO] Failed to open PCD file: {}", pcd_path);
        return mesh;
    }

    PCDMetadata meta;
    std::string line;

    // Header Parsing
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "VERSION") {
            iss >> meta.version;
        }
        else if (token == "FIELDS") {
            while (iss >> token) {
                PCDField f;
                f.field_name = ParseFieldName(token);
                meta.fields.push_back(f);
            }
        } 
        else if (token == "SIZE") {
            for (auto& f : meta.fields) { 
                uint32_t s; iss >> s; 
                f.size = static_cast<uint8_t>(s); 
            }
        } 
        else if (token == "TYPE") {
            for (auto& f : meta.fields) { 
                char t; iss >> t; 
                f.type = ParseDimensionType(t); 
            }
        } 
        else if (token == "COUNT") {
            for (auto& f : meta.fields) { iss >> f.count; }
        } 
        else if (token == "WIDTH") {
            iss >> meta.width;
        }
        else if (token == "HEIGHT") {
            iss >> meta.height;
        }
        else if (token == "VIEWPOINT") {
            for (int i = 0; i < 7; ++i) { iss >> meta.viewpoint[i]; }
        }
        else if (token == "POINTS") {
            iss >> meta.points;
        } 
        else if (token == "DATA") {
            std::string dt_str;
            iss >> dt_str;
            meta.data_type = ParseDataType(dt_str);
            break; // Stop header parsing, data payload begins next
        }
    }
    if (meta.points == 0) {
        LOG_ERROR("[PcdIO] PCD file contains no points or header is invalid: {}", pcd_path);
        return mesh;
    }

    // Field Mapping and Memory Offseting
    int point_stride = 0;
    int x_idx = -1, y_idx = -1, z_idx = -1, rgb_idx = -1, intensity_idx = -1;
    for (size_t i = 0; i < meta.fields.size(); ++i) {
        PCDField& f = meta.fields[i];
        f.byte_offset = point_stride;
        point_stride += (f.size * f.count);

        if (f.field_name == PCDFieldName::UNKNOWN) {
            LOG_WARN("[PcdIO] Unknown field in PCD file ignored.");
            continue;
        }

        switch (f.field_name) {
            case PCDFieldName::X: x_idx = i; break;
            case PCDFieldName::Y: y_idx = i; break;
            case PCDFieldName::Z: z_idx = i; break;
            case PCDFieldName::RGB: rgb_idx = i; break;
            case PCDFieldName::INTENSITY: intensity_idx = i; break;
            default: break;
        }
    }
    if (x_idx == -1 || y_idx == -1 || z_idx == -1) {
        LOG_ERROR("[PcdIO] PCD file missing required x, y, z fields: {}", pcd_path);
        return mesh;
    }


    // Data Payload Parsing
    mesh.positions.reserve(meta.points);
    if (rgb_idx != -1 || intensity_idx != -1) {
        mesh.colors.reserve(meta.points);
    }
    // ASCII Data Type
    if (meta.data_type == PCDDataType::ASCII) {
        for (uint32_t i = 0; i < meta.points; ++i) {
            if (!std::getline(file, line)) break;
            std::istringstream iss(line);
            
            float x = 0, y = 0, z = 0, intensity = 1.0f;
            uint32_t rgb = 0xFFFFFFFF; 

            for (size_t f = 0; f < meta.fields.size(); ++f) {
                if (f == x_idx) iss >> x;
                else if (f == y_idx) iss >> y;
                else if (f == z_idx) iss >> z;
                else if (f == intensity_idx) iss >> intensity;
                else if (f == rgb_idx) {
                    if (meta.fields[f].type == PCDDimensionType::F) {
                        float f_rgb; iss >> f_rgb;
                        std::memcpy(&rgb, &f_rgb, sizeof(uint32_t));
                    } else {
                        iss >> rgb;
                    }
                }
                else {
                    std::string dummy; iss >> dummy; 
                }
            }
            
            mesh.positions.push_back(glm::vec3(x, y, z));
            
            if (rgb_idx != -1) {
                mesh.colors.push_back(UnpackPCDColor(rgb));
            } else if (intensity_idx != -1) {
                float v = NormalizeIntensity(intensity, PCDDimensionType::F, 4);
                mesh.colors.push_back(glm::vec4(v, v, v, 1.0f));
            }
        }
    } 

    // BINARY Data Type
    else if (meta.data_type == PCDDataType::BINARY) {
        std::vector<char> point_buffer(point_stride);
        for (uint32_t i = 0; i < meta.points; ++i) {
            if (!file.read(point_buffer.data(), point_stride)) break;

            const char* buf = point_buffer.data();
            float x = ExtractBinaryValue(buf + meta.fields[x_idx].byte_offset, meta.fields[x_idx].type, meta.fields[x_idx].size);
            float y = ExtractBinaryValue(buf + meta.fields[y_idx].byte_offset, meta.fields[y_idx].type, meta.fields[y_idx].size);
            float z = ExtractBinaryValue(buf + meta.fields[z_idx].byte_offset, meta.fields[z_idx].type, meta.fields[z_idx].size);
            
            mesh.positions.push_back(glm::vec3(x, y, z));

            if (rgb_idx != -1) {
                uint32_t rgb = *reinterpret_cast<const uint32_t*>(buf + meta.fields[rgb_idx].byte_offset);
                mesh.colors.push_back(UnpackPCDColor(rgb));
            } else if (intensity_idx != -1) {
                float raw_val = ExtractBinaryValue(buf + meta.fields[intensity_idx].byte_offset, meta.fields[intensity_idx].type, meta.fields[intensity_idx].size);
                float v = NormalizeIntensity(raw_val, meta.fields[intensity_idx].type, meta.fields[intensity_idx].size);
                mesh.colors.push_back(glm::vec4(v, v, v, 1.0f));
            }
        }
    } 

    // BINARY_COMPRESSED Data Type
    else if (meta.data_type == PCDDataType::BINARY_COMPRESSED) {
        LOG_ERROR("[PcdIO] binary_compressed (LZF) is currently not supported without external libraries: {}", pcd_path);
    } 

    return mesh;
}


} // namespace rrl::io
