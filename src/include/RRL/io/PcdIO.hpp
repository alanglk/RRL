// RRL/src/include/RRLio/PcdIO.hpp


#include "RRL/asset/MeshAsset.hpp"
#include <cstdint>
#include <string>
#include <vector>


namespace rrl::io {

constexpr char PCD_SUPPORTED_VERSION[3] = ".7";

/**
 * @brief Specifies the type of each point dimension.
 */
enum class PCDDimensionType : uint8_t {
    I,      // Signed types:        int8 (char), int16 (short) int32 (int)
    U,      // Unsigned types:      uint8 (uchar), uint16 (ushort), uint32 (uint)
    F       // Float types
};

/**
 * @brief Specifies the name of each dimension/field that a point can have.
 */
enum class PCDFieldName : uint8_t {
    UNKNOWN,    // Unknown dimension -> Logs warning and it is unparsed

    X,          // X dimension
    Y,          // Y dimension
    Z,          // Z dimension
    RGB,        // RGB color dimension
    INTENSITY   // LiDAR return intensity
};

/**
 * @brief PCD Metadata field.
 */
struct PCDField {
    PCDFieldName field_name;                        // Name of the fields
    uint8_t size = 0;                               // Size of each point dimension
    PCDDimensionType type = PCDDimensionType::I;    // Specifies the type of each point dimension
    int count = 1;
    int byte_offset = 0;
};

/**
 * @brief Specifies how the data payload is saved.
 * It can be "ascii", "binary", or "binary_compressed".
 */
enum class PCDDataType: uint8_t {
    ASCII,              // Each point is a new line with ASCII human-friendly values.
    BINARY,             // Direct copy of the in-memory vector. Using mmap/munmap operations.
    BINARY_COMPRESSED   // Not implemented yet.
};

/**
 * @brief Represents the parsed header data of a .PCD file.
 *  # .PCD v.7 - Point Cloud Data file format
 *  VERSION .7
 *  FIELDS x y z rgb
 *  SIZE 4 4 4 4
 *  TYPE F F F F
 *  COUNT 1 1 1 1
 *  WIDTH 213
 *  HEIGHT 1
 *  VIEWPOINT 0 0 0 1 0 0 0
 *  POINTS 213
 *  DATA ascii
 */
struct PCDMetadata {
    std::string version;
    std::vector<PCDField> fields;
    
    uint32_t width = 0;
    uint32_t height = 0;
    
    // Translation (tx, ty, tz) and Quaternion (qw, qx, qy, qz)
    float viewpoint[7] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}; 
    
    uint32_t points = 0;
    PCDDataType data_type; 
};


/**
 * @brief Function to load both ASCII and binary pcd point cloud files.
 */
asset::MeshAsset LoadPCDFile(const std::string& pcd_path);


} // namespace rrl::io
