// RRL/src/rhi/opencv/OpenCVRenderer.cpp

#include "RRL/camera/CameraComponents.hpp"
#ifndef  RRL_RHI_OPENCV
    #error "OpenCVRenderer.hpp file must be compiled with OpenCV support."
#endif

#include "RRL/tf/TFComponents.hpp"
#include "RRL/data/MeshData.hpp"

#include "RRL/rhi/RHIBackend.hpp"
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <unordered_map>


namespace rrl::rhi::opencv {


/**
 * @brief Holds the running OpenCV rendering backend context.
 */
struct OpenCVContext {
    RHIConfig config;
    std::unordered_map<RenderTargetHandle, cv::Mat> render_targets;
    RenderTargetHandle next_handle { 1 }; // 0 is reserved for TARGET_MAIN
};


// --- Lifecycle ---------------------------------------------------
static bool Initialize(entt::registry& registry, const RHIConfig& config) {
    // Inject the context into the registry
    auto& ctx = registry.ctx().emplace<OpenCVContext>();
    ctx.config = config;

    // Implicitly create TARGET_MAIN
    ctx.render_targets[TARGET_MAIN] = cv::Mat(config.height, config.width, CV_8UC3, cv::Scalar(30, 30, 30));

    if (config.mode == RHIRenderingMode::WINDOW) {
        cv::namedWindow(config.title, cv::WINDOW_AUTOSIZE);
    }
    return true;
}
static void Shutdown(entt::registry& registry) {
    // If the context doesn't exist, there's nothing to clean
    if (!registry.ctx().contains<OpenCVContext>()) return;

    auto& ctx = registry.ctx().get<OpenCVContext>();
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::destroyWindow(ctx.config.title);
    }
    
    // Remove the context entirely, destroying all cv::Mat buffers automatically
    registry.ctx().erase<OpenCVContext>();
}



// --- Render Targets (FBOs) ---------------------------------------
static RenderTargetHandle CreateTarget(entt::registry& registry, uint32_t width, uint32_t height) {
    auto& ctx = registry.ctx().get<OpenCVContext>();
    
    RenderTargetHandle handle = ctx.next_handle++;
    ctx.render_targets[handle] = cv::Mat(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    return handle;
}

static void DestroyTarget(entt::registry& registry, RenderTargetHandle handle) {
    if (handle == TARGET_MAIN) return; // Never destroy the main screen buffer directly
    
    auto& ctx = registry.ctx().get<OpenCVContext>();
    ctx.render_targets.erase(handle);
}



// --- Rendering ---------------------------------------------------
static void RenderFrame(entt::registry& registry) {
    auto& ctx = registry.ctx().get<OpenCVContext>();

    // Clear all active targets
    for (auto& [handle, mat] : ctx.render_targets) {
        mat.setTo(cv::Scalar(30, 30, 30));
    }

    // Rendering logic

    // Cache all renderable objects 
    auto mesh_view = registry.view<tf::TFWorldTransformComponent, data::MeshData>();

    // Each camera is a render pass
    auto cam_view = registry.view<camera::CameraComponent, camera::CameraRuntimeComponent>();
    for (auto cam_entity : cam_view) {
        const auto& cam = cam_view.get<camera::CameraComponent>(cam_entity);
        const auto& cam_rt = cam_view.get<camera::CameraRuntimeComponent>(cam_entity);

        // Find the appropriate output matrix for this camera's target FBO
        auto target_it = ctx.render_targets.find(cam.target_fbo);
        if (target_it == ctx.render_targets.end()) {
            continue; // FBO does not exists. Skip
        }
        cv::Mat& render_target = target_it->second;

        // Screen mapping constants based on this specific target's resolution
        float half_w = static_cast<float>(render_target.cols) * 0.5f;
        float half_h = static_cast<float>(render_target.rows) * 0.5f;

        // Mesh drawing
        for (auto mesh_entity : mesh_view) {
            const auto& world_tf = mesh_view.get<tf::TFWorldTransformComponent>(mesh_entity);
            const auto& mesh = mesh_view.get<data::MeshData>(mesh_entity);

            // Pre-compute the final Model-View-Projection (MVP) matrix
            glm::mat4 mvp = cam_rt.view_projection_matrix * world_tf.matrix;

            // Lambda to project a 3D point in Local Coordinate System (LCS) to 2D pixel coordinates
            auto project_vertex = [&](const glm::vec3& local_pos) -> cv::Point {
                glm::vec4 clip_space = mvp * glm::vec4(local_pos, 1.0f);
                
                // Near plane clipping guard (if W <= 0, it's behind or right on the camera plane)
                if (clip_space.w <= 0.0001f) {
                    return cv::Point(-1, -1);
                }
                
                // Perspective Divide -> NDC Space
                glm::vec3 ndc = glm::vec3(clip_space) / clip_space.w;

                // Viewport Transform (NDC [-1, 1] to Pixels [0, width/height])
                int px = static_cast<int>((ndc.x + 1.0f) * half_w);
                int py = static_cast<int>((ndc.y + 1.0f) * half_h);

                return cv::Point(px, py);
            };

            
            // Rasterization
            
            // Point Clouds
            if (mesh.topology == data::MeshTopology::POINTS) {
                for (size_t i = 0; i < mesh.positions.size(); ++i) {
                    cv::Point p = project_vertex(mesh.positions[i]);
                    
                    // Frustum boundary clipping check
                    if (p.x >= 0 && p.x < render_target.cols && p.y >= 0 && p.y < render_target.rows) {
                        // Check if the mesh contains vertex color attributes, fallback to green
                        cv::Scalar color(0, 255, 0); // BGR
                        if (!mesh.colors.empty() && i < mesh.colors.size()) {
                            const auto& c = mesh.colors[i];
                            color = cv::Scalar(
                                static_cast<int>(c.z * 255.0f), // B
                                static_cast<int>(c.y * 255.0f), // G
                                static_cast<int>(c.x * 255.0f)  // R
                            );
                        }
                        cv::circle(render_target, p, 1, color, -1);
                    }
                }
            } 
            // Indexed Wireframe Meshes / Lines
            else if ((mesh.topology == data::MeshTopology::TRIANGLES || mesh.topology == data::MeshTopology::LINES) 
                     && !mesh.indices.empty()) 
            {
                // Step size depends on topology
                size_t step = (mesh.topology == data::MeshTopology::TRIANGLES) ? 3 : 2;
                
                for (size_t i = 0; i < mesh.indices.size(); i += step) {
                    // Triangle wireframe
                    if (step == 3 && i + 2 < mesh.indices.size()) {
                        cv::Point p1 = project_vertex(mesh.positions[mesh.indices[i]]);
                        cv::Point p2 = project_vertex(mesh.positions[mesh.indices[i + 1]]);
                        cv::Point p3 = project_vertex(mesh.positions[mesh.indices[i + 2]]);

                        if (p1.x != -1 && p2.x != -1 && p3.x != -1) {
                            cv::Scalar color(200, 200, 200);
                            cv::line(render_target, p1, p2, color, 1);
                            cv::line(render_target, p2, p3, color, 1);
                            cv::line(render_target, p3, p1, color, 1);
                        }
                    } 
                    // Direct segment lines
                    else if (step == 2 && i + 1 < mesh.indices.size()) {
                        cv::Point p1 = project_vertex(mesh.positions[mesh.indices[i]]);
                        cv::Point p2 = project_vertex(mesh.positions[mesh.indices[i + 1]]);

                        if (p1.x != -1 && p2.x != -1) {
                            cv::line(render_target, p1, p2, cv::Scalar(100, 255, 100), 1);
                        }
                    }
                }
            }
        }
    }



    // Present only the MAIN target to the window
    if (ctx.config.mode == RHIRenderingMode::WINDOW) {
        cv::imshow(ctx.config.title, ctx.render_targets[TARGET_MAIN]);
        cv::waitKey(1);
    }
}
static data::ImageData GetTargetImage(entt::registry& registry, RenderTargetHandle handle) {
    data::ImageData img;
    auto& ctx = registry.ctx().get<OpenCVContext>();

    if (ctx.render_targets.find(handle) == ctx.render_targets.end()) return img;

    const cv::Mat& mat = ctx.render_targets[handle];
    
    img.width = mat.cols;
    img.height = mat.rows;
    img.channels = data::ImageChannelLayout::CH_3;
    img.data_type = data::ImageDataType::UINT8;
    img.color_layout = data::ImageColorLayout::BGR;

    size_t total_bytes = mat.total() * mat.elemSize();
    img.data.assign(mat.datastart, mat.datastart + total_bytes);

    return img;
}



// --- Creation ----------------------------------------------------
RHIBackend CreateBackend() {
    RHIBackend backend;
    backend.type                = RHIBackendType::OPENCV;
    backend.Initialize          = Initialize;
    backend.Shutdown            = Shutdown;
    backend.RenderFrame         = RenderFrame;
    backend.CreateRenderTarget  = CreateTarget;
    backend.DestroyRenderTarget = DestroyTarget;
    backend.GetTargetImage      = GetTargetImage;
    return backend;
}


} // namespace rrl::rhi::opencv 
