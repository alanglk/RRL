// RRL/src/include/RRL/tf/TFComponents.hpp
#pragma once

#include "RRL/tf/TransformTree.hpp"

namespace rrl::tf {


struct TFRelationshipComponent {
    entt::entity parent         { entt::null }; // parent Node          (null = root node)
    entt::entity first_child    { entt::null }; // first child          (depth iteration)
    entt::entity next_sibling   { entt::null }; // next depht sibling   (breadth iteration)
    entt::entity prev_sibling   { entt::null }; // prev depht sibling   (breadth iteration)
    
    uint32_t depth { 0 };               // Node depth
    bool has_dirty_children { true };   // Keeps track if we need to iterate down this branch

    TFDependencyPolicy dependency_policy {TFDependencyPolicy::CASCADE_DELETE};
};
    

/**
 * @brief World origin final matrix transform (ISO8855 CS)
 */
struct TFWorldTransformComponent {
    glm::mat4 matrix{1.0f}; // Identity matrix
};


/**
 * @brief Local relative transform (ISO8855 CS)
 */
struct TFLocalTransformComponent {
public: 
    
    // Getters
    const glm::vec3& GetPosition() const;
    const glm::quat& GetRotation() const;
    const glm::vec3& GetScale() const;
    bool IsDirty() const;
    
    // Setters
    void SetPosition(const glm::vec3& p);
    void SetRotation(const glm::quat& r);
    void SetScale(const glm::vec3& s);
    
    // Called by TF systems to update state
    void ClearDirty();      // List of TF Functions: UpdateNode()
    void SetDirty();

    // Extracts TRS from a raw 4x4 matrix and assigns them locally.
    void SetFromMatrix(const glm::mat4& matrix);

    // Computes the 4x4 TRS transformation matrix
    glm::mat4 GetLocalMatrix() const;


private:
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale {1.0f, 1.0f, 1.0f};
    
    bool is_dirty { false }; // Has this node being updated?
};


}