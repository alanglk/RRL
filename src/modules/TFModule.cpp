// RRL/src/modules/THIModule.cpp

#include "RRL/modules/TFModule.hpp"
#include "RRL/tf/TransformTree.hpp"

#include "RRL/EngineContext.hpp"
#include "RRL/EnttCasting.hpp"


namespace rrl {
    

//  RRL::Engine runtime context.
struct EngineContext;


// --- Constructor / Destructor ------------------------------------
TFModule::TFModule(EngineContext* ctx): m_ctx(ctx) {
    rrl::tf::RegisterTFActions(m_ctx->registry);
}
TFModule::~TFModule() {
}




// --- TF Runtime --------------------------------------------------
void TFModule::UpdateTransformTree() {
    rrl::tf::UpdateTransformTree(m_ctx->registry);
}


// --- TF Adding / Updating / Getting ------------------------------
void TFModule::AddTransform(ObjectID world_object, 
                  const glm::vec3& position, 
                  const glm::quat& rotation, 
                  const glm::vec3& scale) {
    rrl::tf::AddTransform(m_ctx->registry, ToEntt(world_object), position, rotation, scale);
}

void TFModule::AddTransform(ObjectID child_obj, ObjectID parent_obj,
                  const glm::vec3& position, 
                  const glm::quat& rotation, 
                  const glm::vec3& scale,
                  rrl::tf::TFDependencyPolicy policy) {
    rrl::tf::AddTransform(m_ctx->registry, ToEntt(child_obj), ToEntt(parent_obj), position, rotation, scale, policy);
}

void TFModule::SetLocalPosition(ObjectID world_object, const glm::vec3& pos) {
    rrl::tf::SetLocalPosition(m_ctx->registry, ToEntt(world_object), pos);
}

void TFModule::SetLocalRotation(ObjectID world_object, const glm::quat& rot) {
    rrl::tf::SetLocalRotation(m_ctx->registry, ToEntt(world_object), rot);
}

void TFModule::SetLocalScale(ObjectID world_object, const glm::vec3& scale) {
    rrl::tf::SetLocalScale(m_ctx->registry, ToEntt(world_object), scale);
}

glm::vec3 TFModule::GetLocalPosition(ObjectID world_object) {
    return rrl::tf::GetLocalPosition(m_ctx->registry, ToEntt(world_object));
}

glm::quat TFModule::GetLocalRotation(ObjectID world_object) {
    return rrl::tf::GetLocalRotation(m_ctx->registry, ToEntt(world_object));
}

glm::vec3 TFModule::GetLocalScale(ObjectID world_object) {
    return rrl::tf::GetLocalScale(m_ctx->registry, ToEntt(world_object));
}

const glm::mat4& TFModule::GetWorldMatrix(ObjectID world_object) {
    return rrl::tf::GetWorldMatrix(m_ctx->registry, ToEntt(world_object));
}


// --- TF Hierarchy ------------------------------------------------
void TFModule::AttachChild(ObjectID parent_object, ObjectID child_object, 
                 rrl::tf::TFDependencyPolicy policy) {
    rrl::tf::AttachChild(m_ctx->registry, ToEntt(parent_object), ToEntt(child_object), policy);
}

void TFModule::DetachChild(ObjectID child_object) {
    rrl::tf::DetachChild(m_ctx->registry, ToEntt(child_object));
}





} // namespace rrl

