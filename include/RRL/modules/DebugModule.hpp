// RRL/include/modules/AssetModule.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/debug/AssetDebugTypes.hpp"
#include "RRL/debug/RHIDebugTypes.hpp"
#include "RRL/debug/TFDebugTypes.hpp"
#include "RRL/debug/SceneDebugTypes.hpp"


namespace rrl {


//  RRL::Engine runtime context.
struct EngineContext;


class RRL_API DebugModule {
public:
    // --- Constructor / Destructor / Rule of Five ---------------------
    explicit DebugModule(EngineContext* ctx);
    ~DebugModule() = default;

    DebugModule(const DebugModule&)               = delete;
    DebugModule& operator=(const DebugModule&)    = delete;
    DebugModule(DebugModule&&)                    = delete;
    DebugModule& operator=(DebugModule&&)         = delete;


    // --- Assets ------------------------------------------------------
    debug::AssetDebugReport<debug::TextureDebugStats, std::string>  GetTextureDebugReport() const;
    debug::AssetDebugReport<debug::MeshDebugStats, std::string>     GetMeshDebugReport() const;
    debug::AssetDebugReport<debug::MaterialDebugStats, std::string> GetMaterialDebugReport() const;


    // --- Scene & TF --------------------------------------------------
    debug::SceneDebugReport GetSceneDebugReport() const;
    debug::TFDebugReport GetTransformTreeDebugReport() const;


    // --- RHI ---------------------------------------------------------
    debug::RHIDebugReport GetRHIDebugReport() const;
    void SetDebugFlag(rhi::RHIDebugFlag flag, bool enable);


    // --- Visual Debugging --------------------------------------------
    /**
     * @brief Spawns a physical wireframe frustum that matches a camera's parameters.
     */
    ObjectID SpawnCameraFrustum(ObjectID camera_object, float visual_length = 2.0f);

    /**
     * @brief Spawns a basic debug grid on the floor.
     */
    ObjectID SpawnDebugGrid(float size = 10.0f, int subdivisions = 10);

private:
    EngineContext* m_ctx { nullptr };
};

} // namespace rrl