// RRL/src/tf/TFComponents.cpp

#include "RRL/tf/TFComponents.hpp"

#include <glm/gtx/matrix_decompose.hpp>

namespace rrl::tf {
    
// Getters
const glm::vec3& TFLocalTransformComponent::GetPosition() const    { return position; }
const glm::quat& TFLocalTransformComponent::GetRotation() const    { return rotation; }
const glm::vec3& TFLocalTransformComponent::GetScale() const       { return scale; }
bool TFLocalTransformComponent::IsDirty() const                    { return is_dirty; }

void TFLocalTransformComponent::SetPosition(const glm::vec3& p)    { position = p; is_dirty = true; }
void TFLocalTransformComponent::SetRotation(const glm::quat& r)    { rotation = r; is_dirty = true; }
void TFLocalTransformComponent::SetScale(const glm::vec3& s)       { scale = s; is_dirty = true; }

void TFLocalTransformComponent::ClearDirty() { is_dirty = false; }
void TFLocalTransformComponent::SetDirty() { is_dirty = true; }

void TFLocalTransformComponent::SetFromMatrix(const glm::mat4& matrix) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, scale, rotation, position, skew, perspective);
    rotation = glm::conjugate(rotation);
    is_dirty = true;
}
glm::mat4 TFLocalTransformComponent::GetLocalMatrix() const {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 r = glm::mat4_cast(rotation);
    glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
    return t * r * s; 
}

}