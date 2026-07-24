// RRL/include/modules/RHIModule.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/asset/ImageAsset.hpp"
#include "RRL/rhi/RHITypes.hpp"


namespace rrl {


//  RRL::Engine runtime context.
struct EngineContext;



class RRL_API RHIModule {
public:

    // --- Constructor / Destructor ------------------------------------
    explicit RHIModule(EngineContext* ctx);
    ~RHIModule();

    // Rule of five (this class is owned by RRLEngine)
    RHIModule(const RHIModule&)             = delete;
    RHIModule& operator=(const RHIModule&)  = delete;
    RHIModule(RHIModule&&)                  = delete;
    RHIModule& operator=(RHIModule&&)       = delete;


    // --- Window ------------------------------------------------------
    /**
     * @brief Create a window template for later instantiation.
     */
    rrl::rhi::RHIWindow CreateWindow(rrl::rhi::RHIWindowType window_type);
    /**
     * @brief Actual window creation
     */
    bool InitializeWindow(rrl::rhi::RHIWindow& window, const char* title, uint32_t w, uint32_t h);
    /**
     * @brief Window event polling
     */
    bool PollWindowEvents(rrl::rhi::RHIWindow& window);
    /**
     * @brief Destroys a RHI window. This also tells the 
     * backend that the window is no longer available.
     */
    void DestroyWindow(rrl::rhi::RHIWindow& window);



    // --- Backend -----------------------------------------------------
    /**
     * @brief Swaps the active rendering backend at runtime.
     * If a backend is already active, it safely shuts it down first.
     * Returns false if the requested backend is not available.
     */
    bool LoadBackend(rrl::rhi::RHIBackendType target_backend);
    /**
     * @brief Returns the currently active backend type.
     */
    rrl::rhi::RHIBackendType GetCurrentBackendType();



    // --- Lifecycle ---------------------------------------------------
    /**
     * @brief Initializes the currently loaded rendering backend.
     * Automatically provisions the TARGET_MAIN render target setting 
     * the rendering dimensions to the provided window handle.
     */
    bool Initialize(const rrl::rhi::RHIWindow* window);
    /**
     * @brief Initializes the currently loaded rendering backend.
     */
    bool Initialize(uint32_t render_width, uint32_t render_height, const rrl::rhi::RHIWindow* window);
    /**
     * @brief Cleans up memory and contexts
     */
    void Shutdown();
    /**
     * @brief Iterates the current scene, projects 3D data, and dispatches draw calls 
     * across all active cameras and render targets.
     */
    void RenderFrame();
    /**
     * @brief Reads rendered data from the RHI back to CPU RAM.
     */
    rrl::asset::ImageAsset GetTargetImage(rrl::rhi::ResourceID id = rrl::rhi::TARGET_MAIN);



    // --- Render Targets (FBOs) ---------------------------------------
    /**
     * @brief Requests the backend to create an off-screen render target bound to a Virtual ResourceID.
     */
    void CreateRenderTarget(rrl::rhi::ResourceID id, const rrl::rhi::RenderTargetDescriptor& desc);
    /**
     * @brief Destroys an off-screen render target and frees its memory via its Virtual ResourceID.
     * Note: TARGET_MAIN cannot be destroyed through this function.
     */
    void DestroyRenderTarget(rrl::rhi::ResourceID id);



private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl


