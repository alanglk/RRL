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
    rrl::asset::ImageAsset GetTargetImage(rrl::rhi::ResourceID id = rrl::rhi::TARGET_MAIN, rrl::rhi::RHIRenderAttachmentSemantic semantic = rrl::rhi::RHIRenderAttachmentSemantic::COLOR, uint32_t array_index = 0);
    /**
     * @brief Configures which render target, semantic layer, and array slice is presented 
     * to the active window at the end of RenderFrame().
     * @warning This is a temporary output router before the full Render Dependency Graph (RDG) is implemented.
     */
    void SetPresentationTarget(rrl::rhi::ResourceID id = rrl::rhi::TARGET_MAIN, rrl::rhi::RHIRenderAttachmentSemantic semantic = rrl::rhi::RHIRenderAttachmentSemantic::COLOR, uint32_t array_index = 0);




    // --- Render Targets (FBOs) ---------------------------------------
    /**
     * @brief Explicitly allocates physical RHI texture memory bound to a Virtual ResourceID.
     * Required when setting up shared memory buffers (like Texture Arrays for 
     * multi-camera stitching). For standard 2D targets, this is optional as 
     * CreateRenderTarget will lazily allocate missing textures automatically.
     * @warning is_depht is not used and will be removed!!
     */
    void AllocateRenderTargetTexture(rrl::rhi::ResourceID id, uint32_t width, uint32_t height, 
                           rrl::asset::ImageAssetType data_type, rrl::asset::ImageChannelLayout channels, 
                           bool is_depth, uint32_t array_layers = 1);
    /**
     * @brief Creates an off-screen render target (FBO) routing table bound to a Virtual ResourceID.
     * If the descriptor requests a slice of a Texture Array (e.g., for multi-camera stitching), 
     * the backing memory must have been explicitly allocated first via AllocateRenderTargetTexture. 
     * Lazy allocation is strictly forbidden for Texture Arrays.
     */
    void CreateRenderTarget(rrl::rhi::ResourceID id, const rrl::rhi::RHIRenderTargetDescriptor& desc);
    /**
     * @brief Destroys an off-screen render target (FBO) via its Virtual ResourceID.
     * @note: This destroys the FBO routing table. If backing textures were shared 
     * or explicitly allocated, their memory remains intact until explicitly destroyed.
     * @note: TARGET_MAIN cannot be destroyed through this function.
     */
    void DestroyRenderTarget(rrl::rhi::ResourceID id);

    /**
     * @brief Destroys a render target texture and frees its physical memory via its Virtual ResourceID.
     * Use this to clean up shared memory buffers explicitly allocated via AllocateRenderTargetTexture.
     * @note This does not enable to destroy Texture assets. Use the asset module for that.
     * @warning Ensure you call DestroyRenderTarget on any FBOs using this texture BEFORE destroying it, 
     * otherwise the FBOs will be left with dangling pointers.
     */
    void DestroyRenderTargetTexture(rrl::rhi::ResourceID id);


private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl


