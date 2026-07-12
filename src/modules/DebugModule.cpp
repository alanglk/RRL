// RRL/src/modules/DebugModule.cpp

#include "RRL/modules/DebugModule.hpp"
#include "RRL/EngineContext.hpp"

#include "RRL/debug/AssetDebugger.hpp"
#include "RRL/debug/RHIDebugger.hpp"
#include "RRL/debug/SceneDebugger.hpp"
#include "RRL/debug/TFDebugger.hpp"

#include "RRL/EnttCasting.hpp"

namespace rrl {

// --- Constructor -------------------------------------------------
DebugModule::DebugModule(EngineContext* ctx) : m_ctx(ctx) {
}


// --- Assets ------------------------------------------------------
debug::AssetDebugReport<debug::TextureDebugStats, std::string> DebugModule::GetTextureDebugReport() const {
    return rrl::debug::asset::GetTextureDebugReport(m_ctx->registry);
}

debug::AssetDebugReport<debug::MeshDebugStats, std::string> DebugModule::GetMeshDebugReport() const {
    return rrl::debug::asset::GetMeshDebugReport(m_ctx->registry);
}

debug::AssetDebugReport<debug::MaterialDebugStats, std::string> DebugModule::GetMaterialDebugReport() const {
    return rrl::debug::asset::GetMaterialDebugReport(m_ctx->registry);
}


// --- Scene & TF --------------------------------------------------
debug::SceneDebugReport DebugModule::GetSceneDebugReport() const {
    return rrl::debug::scene::GetSceneDebugReport(m_ctx->registry);
}

debug::TFDebugReport DebugModule::GetTransformTreeDebugReport() const {
    return rrl::debug::tf::GetTransformTreeDebugReport(m_ctx->registry);
}


// --- RHI ---------------------------------------------------------
debug::RHIDebugReport DebugModule::GetRHIDebugReport() const {
    return rrl::debug::rhi::GetRHIDebugReport(m_ctx->registry);
}

void DebugModule::SetDebugFlag(rhi::RHIDebugFlag flag, bool enable) {
    rrl::debug::rhi::SetDebugFlag(m_ctx->registry, flag, enable);
}


// --- Visual Debugging --------------------------------------------
ObjectID DebugModule::SpawnCameraFrustum(ObjectID camera_object, float visual_length) {
    entt::entity frustum_entity = rrl::debug::rhi::SpawnCameraFrustum(m_ctx->registry, ToEntt(camera_object), visual_length);
    return ToObjectID(frustum_entity);
}

ObjectID DebugModule::SpawnDebugGrid(float size, int subdivisions) {
    entt::entity grid_entity = rrl::debug::rhi::SpawnDebugGrid(m_ctx->registry, size, subdivisions);
    return ToObjectID(grid_entity);
}


} // namespace rrl
