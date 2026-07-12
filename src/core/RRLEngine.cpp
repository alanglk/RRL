// RRL/src/RRLEngine.cpp

#include "RRL/RRLEngine.hpp"
#include "RRL/EngineContext.hpp"

namespace rrl {

// --- Constructor -------------------------------------------------
Engine::Engine() 
    : m_ctx(std::make_unique<EngineContext>()),
      rhi(m_ctx.get()),
      asset(m_ctx.get()),
      tf(m_ctx.get()),
      scene(m_ctx.get()),
      camera(m_ctx.get()),
      debug(m_ctx.get())
{
    // The engine is fully booted: RAII
}


// --- Destructor --------------------------------------------------
Engine::~Engine() {
    // Because of the declaration order in RRLEngine.hpp, 
    // C++ destroys these in reverse order:
    //
    // 1. debug.~DebugModule()
    // 2. camera.~CameraModule()
    // 3. scene.~SceneModule()
    // 4. tf.~TFModule()
    // 5. asset.~AssetModule() -> Destroys all assets, calling GPU delete functions.
    // 6. rhi.~RHIModule()     -> Shuts down the backend and window contexts.
    // 7. m_ctx.~unique_ptr()  -> Destroys the entt::registry and frees all raw ECS memory.
}

} // namespace rrl