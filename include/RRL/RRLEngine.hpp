// RRL/include/RRLEngine.hpp
#pragma once

#include <memory>
#include <RRL/rrl_export.h>

#include "RRL/modules/AssetModule.hpp"
#include "RRL/modules/CameraModule.hpp"
#include "RRL/modules/DebugModule.hpp"
#include "RRL/modules/RHIModule.hpp"
#include "RRL/modules/SceneModule.hpp"
#include "RRL/modules/TFModule.hpp"


namespace rrl {
    
// Fordward declaration to implementation of the
//  RRL::Engine runtime context.
struct EngineContext;


class RRL_API Engine {
private:
    std::unique_ptr<EngineContext> m_ctx { nullptr };

public:
    // Subsystems API 
    RHIModule rhi;
    AssetModule asset;
    TFModule tf;
    SceneModule scene;
    CameraModule camera;
    DebugModule debug;
    

    // Constructor / Destructor 
    Engine();
    ~Engine();
};


} // namespace rrl
