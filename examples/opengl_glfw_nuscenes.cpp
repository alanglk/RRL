// examples/opengl_glfw_nuscenes.cpp

#include "RRL/camera/CameraConventions.hpp"
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <FLogging/FLogging.hpp>


// Olex includes
#include <olex/interface/nuscenes/NuDataset.hpp>
using namespace olex::iface::nuscenes;


// RRL Engine Modules
#include <RRL/RRLTypes.hpp>
#include <RRL/io/PrefabIO.hpp>
#include <RRL/io/ImageIO.hpp>
#include <RRL/RRLEngine.hpp>



// --- Global variables --------------------------------------------
const std::string nu_path       = "assets/datasets/nuscenes";
const std::string nu_version    = "v1.0-mini";          // Dataset version
const std::string nu_scene_name = "scene-0061";         // Scene name
const std::vector<std::string> nu_sensor_names = {      // Used sensors for rendering
    "CAM_FRONT", "CAM_FRONT_LEFT", "CAM_FRONT_RIGHT", "LIDAR_TOP"
};


// --- NuDataset -> RRL Helpers ------------------------------------
/**
 * @brief NuDataset sensor extracted data for an specific frame.
 * This bridges the NuDataset -> RRL gap.
 */
struct ExtractedSensorData {
    std::string channel_name;
    std::string filename;
    
    glm::vec3 translation {0.0f };  // Local transform (Sensor -> Ego)
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    bool is_camera;
    std::optional<rrl::camera::CameraModelVariant> camera_model;
};
/**
 * @brief NuDataset frame extracted data.
 * This bridges the NuDataset -> RRL gap.
 */
struct ExtractedFrameData {
    glm::vec3 ego_translation; // Global transform (Ego -> World)
    glm::quat ego_rotation;
    std::unordered_map<std::string, ExtractedSensorData> sensors; // channel_name -> ExtractedSensorData
};
/**
 * @brief Extracts a single frame's synchronized ego pose and sensor calibrations.
 * @param nu_dataset The loaded NuScenes dataset instance.
 * @param frame_token The specific SampleToken (frame) to extract.
 * @param nu_sensors The list of SensorTokens to process for this frame.
 * @return ExtractedFrameData The structures ready for RRL ingestion.
 */
ExtractedFrameData ExtractFrameData(
    const olex::iface::nuscenes::NuDataset& nu_dataset,
    const olex::iface::nuscenes::SampleToken& frame_token,
    const std::vector<olex::iface::nuscenes::SensorToken>& nu_sensors) 
{
    using namespace olex::iface::nuscenes;
    
    ExtractedFrameData frame_data;

    // Frame anchored Ego Pose
    // nuScenes keyframes are anchored to LIDAR_TOP.
    EgoPoseToken anchor_ego_token = NULL_EgoPoseToken;
    SensorToken lidar_token = nu_dataset.GetSensorToken("LIDAR_TOP");
    if (lidar_token != NULL_SensorToken) {
        auto lidar_datas = nu_dataset.GetSensorSampleDataOfSample(lidar_token, frame_token);
        if (!lidar_datas.empty()) {
            const auto& anchor_data = nu_dataset.GetSampleData(lidar_datas.front());
            anchor_ego_token = anchor_data.ego_pose_token;
        }
    }
    if (anchor_ego_token == NULL_EgoPoseToken) {
        auto fallback_datas = nu_dataset.GetSampleDataOfSample(frame_token);
        if (!fallback_datas.empty()) {
            anchor_ego_token = nu_dataset.GetSampleData(fallback_datas.front()).ego_pose_token;
        }
    }

    // Assign the global Ego transform for RRL
    if (anchor_ego_token != NULL_EgoPoseToken) {
        const auto& frame_ego_pose = nu_dataset.GetEgoPose(anchor_ego_token);
        frame_data.ego_rotation = glm::quat(
            static_cast<float>( frame_ego_pose.rotation[0] ), // w
            static_cast<float>( frame_ego_pose.rotation[1] ), // x
            static_cast<float>( frame_ego_pose.rotation[2] ), // y
            static_cast<float>( frame_ego_pose.rotation[3] )  // z
        );
        frame_data.ego_translation = glm::vec3(
            frame_ego_pose.translation[0], 
            frame_ego_pose.translation[1], 
            frame_ego_pose.translation[2]
        );
    }

    // Extract Sensor Data
    for (const auto& sensor_token : nu_sensors) {
        auto sensor_data_tokens = nu_dataset.GetSensorSampleDataOfSample(sensor_token, frame_token);
        
        for (const auto& sample_data_token : sensor_data_tokens) {
            const auto& sample_data = nu_dataset.GetSampleData(sample_data_token);
            const auto& calib_sensor = nu_dataset.GetCalibratedSensor(sample_data.calibrated_sensor_token);
            const auto& sensor = nu_dataset.GetSensor(calib_sensor.sensor_token);
            
            ExtractedSensorData extracted_sensor;
            extracted_sensor.channel_name = sensor.channel;
            extracted_sensor.filename = sample_data.filename;
            
            // Sensor Extrinsics
            extracted_sensor.rotation = glm::quat(
                static_cast<float>( calib_sensor.rotation[0] ), // w
                static_cast<float>( calib_sensor.rotation[1] ), // x
                static_cast<float>( calib_sensor.rotation[2] ), // y
                static_cast<float>( calib_sensor.rotation[3] )  // z
            );
           extracted_sensor.translation = glm::vec3(
                static_cast<float>( calib_sensor.translation[0] ),
                static_cast<float>( calib_sensor.translation[1] ),
                static_cast<float>( calib_sensor.translation[2] )
            );

            // Sensor Intrinsics (Cameras Only)
            if (!calib_sensor.is_camera) {
                frame_data.sensors[extracted_sensor.channel_name] = extracted_sensor;
                continue;
            }
            // // NuScenes Camera points +Z forward, +Y down.
            // // We rotate it 90 degrees around X to make it point forward in a Y-Up world.
            // glm::quat correction_rot = glm::quat(0.7071f, -0.7071f, 0.0f, 0.0f); // -90 deg on X
            // extracted_sensor.rotation = extracted_sensor.rotation * correction_rot;
            const auto& K = calib_sensor.camera_intrinsic;

            rrl::camera::PinholeModel camera;
            camera.fx = static_cast<float>( K[0] );
            camera.fy = static_cast<float>( K[4] );
            camera.cx = static_cast<float>( K[2] ); // Flat row-major mapping
            camera.cy = static_cast<float>( K[5] );
            camera.width_px  = sample_data.width;
            camera.height_px = sample_data.height;
            camera.z_near = 0.1f;
            camera.z_far = 5000.0f;
            extracted_sensor.camera_model = camera;
            frame_data.sensors[extracted_sensor.channel_name] = extracted_sensor;
        }
    }

    return frame_data;
}
/**
 * @brief Load a Nuscene saved point cloud.
 */
rrl::asset::MeshAsset LoadNuScenesCloud(const std::string& filepath) {
    rrl::asset::MeshAsset mesh;
    mesh.topology = rrl::asset::MeshTopology::POINTS;

    // Open at the end to immediately get file size
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("[NuScenes] Failed to open NuScenes cloud file: {}", filepath);
        return mesh;
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size <= 0) {
        LOG_ERROR("[NuScenes] NuScenes cloud file is empty: {}", filepath);
        return mesh;
    }

    // NuScenes LiDAR: 5 floats (x, y, z, intensity, ring_index) = 20 bytes
    // NuScenes Radar: 18 floats (x, y, z, dyn_prop, id, rcs, vx, vy, ...) = 72 bytes
    int floats_per_point = 0;
    if (file_size % 20 == 0) {
        floats_per_point = 5;
    } else if (file_size % 72 == 0) {
        floats_per_point = 18;
    } else {
        LOG_ERROR("[NuScenes] File size ({}) does not match NuScenes LiDAR (20-byte stride) or Radar (72-byte stride): {}", file_size, filepath);
        return mesh;
    }

    size_t num_points = file_size / (floats_per_point * sizeof(float));
    mesh.positions.reserve(num_points);
    mesh.colors.reserve(num_points);

    // Read entire file in one go for maximum speed
    std::vector<float> buffer(num_points * floats_per_point);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        LOG_ERROR("[NuScenes] Failed to read data from NuScenes file: {}", filepath);
        return mesh;
    }

    // Map memory to engine arrays
    for (size_t i = 0; i < num_points; ++i) {
        size_t offset = i * floats_per_point;
        
        float x = buffer[offset + 0]; // NuScenes Forward
        float y = buffer[offset + 1]; // NuScenes Left
        float z = buffer[offset + 2]; // NuScenes Up
        mesh.positions.push_back(glm::vec3(x, y, z));

        // For Radar (18), we could map velocity (vx, vy) to colors or just map rcs (index 5)
        // For LiDAR (5), index 3 is intensity
        float intensity = 1.0f;
        if (floats_per_point == 5) {
            intensity = buffer[offset + 3];
        } else if (floats_per_point == 18) {
            intensity = buffer[offset + 5]; // RCS (Radar Cross Section) acts as intensity
        }
        
        // Normalize intensity to 0.0-1.0 range
        float v = (intensity > 1.0f) ? (intensity / 255.0f) : intensity;
        
        // Clamp to prevent blowout in rendering
        if (v > 1.0f) v = 1.0f;
        if (v < 0.0f) v = 0.0f;
        
        mesh.colors.push_back(glm::vec4(v, v, v, 1.0f));
    }

    return mesh;
}






// --- Main entrypoint ---------------------------------------------
int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Debug, flogging::BackendType::StdFormat);
    // LOG_INFO("[RRL Engine] Running Rungholt City on OpenCV Backend");
    // LOG_INFO("[RRL Engine] Shutting down...");

    
    NuDataset nu_dataset;
    nu_dataset.LoadDataset(nu_path, nu_version);
    SceneToken nu_scene_token = nu_dataset.GetSceneToken(nu_scene_name);
    assert(nu_scene_token != NULL_SceneToken);
    Scene nu_scene = nu_dataset.GetScene(nu_scene_token);
    LOG_INFO("[NuDataset] scene name: {} | number of frames: {} | Description: {}",
        nu_scene.name, nu_scene.nbr_samples, nu_scene.description
    );
    
    // Resolve sensor names
    std::vector<SensorToken> nu_sensors;
    for (const auto& sensor_name : nu_sensor_names) {
        SensorToken token = nu_dataset.GetSensorToken(sensor_name);
        if (token != NULL_SensorToken)  nu_sensors.emplace_back(token);
    }


    // Scene frame mapping
    std::unordered_map<uint32_t, SampleToken> frame_to_token_index;
    const auto& frames = nu_dataset.GetSamples(nu_scene_token);
    const uint32_t frame_count = nu_scene.nbr_samples;
    uint32_t frame_idx = 0;
    for (const auto& frame: frames) {
        frame_to_token_index[frame_idx] = frame.token;
        frame_idx++;
    }
    assert(frame_idx == frame_count);
    

    // Initialize RRL engine
    uint32_t window_w = 1280;
    uint32_t window_h = 720;
    rrl::Engine engine;

    auto window = engine.rhi.CreateWindow(rrl::rhi::RHIWindowType::GLFW);
    engine.rhi.InitializeWindow(window, "RRL - Rungholt Instancing Viewer", window_w, window_h);
    engine.rhi.LoadBackend(rrl::rhi::RHIBackendType::OPENGL);
    engine.rhi.Initialize(&window);
    engine.debug.SetDebugFlag(rrl::rhi::RHIDebugFlag::FLAG_AFFINE_INTERPOLATION, true);



    // Load a NuDataset frame into the RRL Engine
    uint32_t target_frame = 0;
    const auto& frame = nu_dataset.GetSample(frame_to_token_index[target_frame]);
    ExtractedFrameData frame_data = ExtractFrameData(nu_dataset, frame.token, nu_sensors);


    // Set Ego Vehicle position
    rrl::ObjectID ego_id = engine.scene.SpawnObject();
    engine.tf.AddTransform(ego_id, frame_data.ego_translation, frame_data.ego_rotation);


    // Allocate cameras rhi data. All cameras have MRT: color and depht
    uint32_t num_cameras = 0;
    uint32_t cam_w = 0, cam_h = 0;
    const rrl::rhi::ResourceID SHARED_COLOR_ARRAY_ID = "shared_color_array";
    const rrl::rhi::ResourceID SHARED_DEPTH_ARRAY_ID = "shared_depth_array";
    for (const auto& [sensor_name, sensor_data] : frame_data.sensors) {
        if (sensor_data.is_camera && sensor_data.camera_model.has_value()) {
            num_cameras++;
            const auto* pinhole = std::get_if<rrl::camera::PinholeModel>(&sensor_data.camera_model.value());
            cam_w = std::max(pinhole->width_px, cam_w);
            cam_h = std::max(pinhole->height_px, cam_h);
        }
    }
    if (num_cameras > 0) {
        // Allocate Color Array (Layered)
        engine.rhi.AllocateRenderTargetTexture(
            SHARED_COLOR_ARRAY_ID, cam_w, cam_h, 
            rrl::asset::ImageAssetType::UINT8, 
            rrl::asset::ImageChannelLayout::CH_4, 
            false, 
            num_cameras
        );
        
        // Allocate Depth Array (Layered)
        engine.rhi.AllocateRenderTargetTexture(
            SHARED_DEPTH_ARRAY_ID, cam_w, cam_h, 
            rrl::asset::ImageAssetType::FLOAT32, 
            rrl::asset::ImageChannelLayout::CH_1, 
            false, 
            num_cameras
        );
    }



    // Add sensor calibration and wire FBOs
    uint32_t current_camera_layer = 0;
    std::unordered_map<std::string, rrl::rhi::ResourceID> camera_fbos;  // Camera FBO ids
    std::unordered_map<std::string, uint32_t> camera_layers;
    std::unordered_map<std::string, rrl::ObjectID> sensor_ids;          // Scene sensor ids
    for (const auto& [sensor_name, sensor_data] : frame_data.sensors) {
        rrl::ObjectID sensor_id = rrl::NULL_OBJECT;
        
        // Cameras
        if (sensor_data.is_camera && sensor_data.camera_model.has_value()) {
            const rrl::rhi::ResourceID cam_fbo_id{2000 + current_camera_layer};
            rrl::rhi::RHIRenderTargetDescriptor fbo_desc;
            fbo_desc.width = cam_w;
            fbo_desc.height = cam_h;
            fbo_desc.color_attachments.push_back({SHARED_COLOR_ARRAY_ID, 
                rrl::asset::ImageAssetType::UINT8, 
                rrl::asset::ImageChannelLayout::CH_4, 
                rrl::rhi::RHIRenderAttachmentSemantic::COLOR,
                true,
                current_camera_layer
            });
            fbo_desc.color_attachments.push_back({SHARED_DEPTH_ARRAY_ID, 
                rrl::asset::ImageAssetType::FLOAT32, 
                rrl::asset::ImageChannelLayout::CH_1, 
                rrl::rhi::RHIRenderAttachmentSemantic::DEPTH,
                true,
                current_camera_layer
            });
            
            // Create the FBO pointing to the allocated Texture Array objects
            engine.rhi.CreateRenderTarget(cam_fbo_id, fbo_desc);
            camera_fbos[sensor_name] = cam_fbo_id;
            camera_layers[sensor_name] = current_camera_layer;
            
            // Spawn scene camera object
            sensor_id = engine.camera.SpawnCamera(
                *sensor_data.camera_model,
                rrl::camera::CameraViewBasis::STANDARD_OPENCV,
                cam_fbo_id,
                rrl::rhi::RHIRenderLayerMask::LAYER_ALL
            );
            sensor_ids[sensor_name] = sensor_id;
            engine.tf.AttachChild(ego_id, sensor_id);
            engine.tf.SetLocalPosition(sensor_id, sensor_data.translation);
            engine.tf.SetLocalRotation(sensor_id, sensor_data.rotation);
            current_camera_layer++;

        } 

        // Other sensors
        else {
            sensor_id = engine.scene.SpawnObject();
            sensor_ids[sensor_name] = sensor_id;
            engine.tf.AddTransform(sensor_id);
            engine.tf.AttachChild(ego_id, sensor_id);
            engine.tf.SetLocalPosition(sensor_id, sensor_data.translation);
            engine.tf.SetLocalRotation(sensor_id, sensor_data.rotation);
        }
    }
    const auto tf_debug_report = engine.debug.GetTransformTreeDebugReport();
    engine.tf.UpdateTransformTree();
    engine.camera.UpdateCameras(rrl::camera::NDC_OPENCV); // Initialize / Update cameras

    // Load Assets and Bind to Scene
    std::unordered_map<std::string, rrl::AssetID> sensor_data_ids;  // Scene sensor actual data ids (for future updates)
    for (const auto& [sensor_name, sensor_data] : frame_data.sensors) {
        std::string full_path = nu_path + "/" + sensor_data.filename;
        
        if (sensor_data.is_camera && sensor_data.camera_model.has_value()) {
            // Load the camera image asset and bind it to the camera background
            rrl::asset::ImageAsset sensor_image = rrl::io::LoadImage(full_path);

            if (sensor_data_ids.count(sensor_name) == 0) {
                rrl::AssetID tex_asset = engine.asset.CreateTexture(sensor_name,  std::move(sensor_image));
                sensor_data_ids[sensor_name] = tex_asset;
            }
            else {
                engine.asset.UpdateTexture(sensor_data_ids[sensor_name], std::move(sensor_image));
            }

            engine.camera.SetCameraBackgroundFlatTexture(sensor_ids[sensor_name], sensor_data_ids[sensor_name]);
        } 
        else if (sensor_name == "LIDAR_TOP") {
            rrl::asset::MeshAsset lidar_data = LoadNuScenesCloud(full_path);
            if (sensor_data_ids.count(sensor_name) == 0) {
                rrl::AssetID lidar_asset = engine.asset.CreateMesh(sensor_name, std::move(lidar_data));
                sensor_data_ids[sensor_name] = lidar_asset;
                engine.asset.BindMesh(sensor_ids[sensor_name], lidar_asset);
            } 
            else {
                engine.asset.UpdateMesh(sensor_data_ids[sensor_name], std::move(lidar_data));
            }
        }
    }


    // std::vector<std::string> camera_targets = {"CAM_FRONT", "CAM_FRONT_RIGHT", "CAM_FRONT_LEFT"};
    // uint32_t camera_target_idx = 0;
    const std::string main_camera_target = "CAM_FRONT";
    engine.rhi.SetPresentationTarget(
        camera_fbos[main_camera_target],
        rrl::rhi::RHIRenderAttachmentSemantic::COLOR, 
        camera_layers[main_camera_target]
    );
    
    LOG_INFO("[RRL Engine] Entering main render loop...");
    while (engine.rhi.PollWindowEvents(window)) {
        engine.tf.UpdateTransformTree();
        engine.camera.UpdateCameras(rrl::camera::NDC_OPENCV);
        
        /*
        engine.rhi.SetPresentationTarget(
            camera_fbos[camera_targets[camera_target_idx]],
            rrl::rhi::RHIRenderAttachmentSemantic::COLOR, 
            camera_layers[camera_targets[camera_target_idx]]
        );
        camera_target_idx++;
        if(camera_target_idx >= camera_targets.size()) camera_target_idx = 0;
        */


        engine.rhi.RenderFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    LOG_INFO("[RRL Engine] Shutting down...");
    engine.rhi.DestroyWindow(window);
    engine.rhi.Shutdown();
    flogging::ResetLogger();
    return 0;
}
