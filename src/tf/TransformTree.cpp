// RRL/src/tf/TransformTree.cpp

#include "RRL/tf/TransformTree.hpp"
#include "RRL/tf/TFComponents.hpp"
#include "entt/entity/fwd.hpp"

#include <glm/gtx/matrix_decompose.hpp>

namespace rrl::tf {
    


// --- TF Runtime --------------------------------------------------

/**
 * @brief Helper to compute the absolute world matrix of a node. 
 * This is called from OnParentDestroy when the REPARENT_TO_WORLD policy is active on an specific child.
 * It is important to compute the world matrix of the child node before its parent gets deleted 
 * as we need to look up all its branched to get a reliable world transform.
 */
glm::mat4 ComputeAbsoluteMatrix(entt::registry& reg, entt::entity node) {
    glm::mat4 abs_matrix(1.0f);
    entt::entity curr = node;
    
    // Walk up to the root, multiplying local matrices
    while (curr != entt::null) {
        const auto& local = reg.get<TFLocalTransformComponent>(curr);
        abs_matrix = local.GetLocalMatrix() * abs_matrix; 
        curr = reg.get<TFRelationshipComponent>(curr).parent;
    }
    return abs_matrix;
}

/**
 * @brief Helper function executed when a TFRelationshipComponent gets destroyed.
 *  We need to update all its siblings depending on their defined TFDependencyPolicy
 */
void OnParentDestroy(entt::registry& registry, entt::entity parent_entity) {
    auto& parent_rel = registry.get<TFRelationshipComponent>(parent_entity);
    
    // Node re-linking
    if (parent_rel.prev_sibling != entt::null) {
        // I have an older sibling; tell them to point to my younger sibling
        registry.get<TFRelationshipComponent>(parent_rel.prev_sibling).next_sibling = parent_rel.next_sibling;
    } else if (parent_rel.parent != entt::null) {
        // I have no older sibling, meaning I am the first_child of my parent
        registry.get<TFRelationshipComponent>(parent_rel.parent).first_child = parent_rel.next_sibling;
    }
    if (parent_rel.next_sibling != entt::null) {
        // I have a younger sibling; tell them to point to my older sibling
        registry.get<TFRelationshipComponent>(parent_rel.next_sibling).prev_sibling = parent_rel.prev_sibling;
    }


    // Manage children
    entt::entity current_child = parent_rel.first_child;
    while (current_child != entt::null) {
        auto& child_rel = registry.get<TFRelationshipComponent>(current_child);
        entt::entity next_sibling = child_rel.next_sibling; 

        // Cascade deletion
        if (child_rel.dependency_policy == TFDependencyPolicy::CASCADE_DELETE) {
            registry.destroy(current_child);
        }

        // Reparent this node to WORLD
        else if (child_rel.dependency_policy == TFDependencyPolicy::REPARENT_TO_WORLD) {

            glm::mat4 true_world_matrix = ComputeAbsoluteMatrix(registry, current_child);
            if (registry.all_of<TFLocalTransformComponent>(current_child)) {
                auto& local = registry.get<TFLocalTransformComponent>(current_child);
                // This flags the LTF it so all its childs gets updated on the next UpdateTransformTree call
                local.SetFromMatrix(true_world_matrix); 
            }
            
            // Now it becomes a root node
            child_rel.parent = entt::null;
            child_rel.prev_sibling = entt::null;
            child_rel.next_sibling = entt::null;
            child_rel.depth = 0;
        }

        // Advance to the cached sibling
        current_child = next_sibling; 
    }
}
void RegisterTFActions(entt::registry& registry) {
    registry.on_destroy<TFRelationshipComponent>().connect<&OnParentDestroy>();
}


/**
 * @brief Helper recursive function to perform node world matrixs updates only when it is really needed.
 * This function ensures the local and world transform keeps in sync between al TF nodes. 
 */
void UpdateNode(entt::registry& registry, entt::entity entity, const glm::mat4& parent_matrix, bool parent_changed) {
    auto& local = registry.get<TFLocalTransformComponent>(entity);
    auto& rel = registry.get<TFRelationshipComponent>(entity);
    auto& world = registry.get<TFWorldTransformComponent>(entity);

    // Update the world matrix if the parent matrix or the local node tf have changed
    bool matrix_changed = parent_changed || local.IsDirty(); 
    if (matrix_changed) {
        world.matrix = parent_matrix * local.GetLocalMatrix();
        local.ClearDirty(); // Reset the flag
    }

    // Only traverse down if this matrix changed or a deeper child moved
    if (matrix_changed || rel.has_dirty_children) {
        entt::entity current_child = rel.first_child;
        while (current_child != entt::null) {
            UpdateNode(registry, current_child, world.matrix, matrix_changed);
            current_child = registry.get<TFRelationshipComponent>(current_child).next_sibling;
        }
    }

    // Reset branch tracking for the next frame
    rel.has_dirty_children = false;
}
void UpdateTransformTree(entt::registry& registry) {
    auto view = registry.view<TFRelationshipComponent>();

    // We only iterate through roots (entities with no parent).
    for (auto entity : view) {
        if (view.get<TFRelationshipComponent>(entity).parent == entt::null) {
            UpdateNode(registry, entity, glm::mat4(1.0f), false);
        }
    }
}



// --- TF Adding / Updating / Getting ------------------------------



/**
 * @brief Helper to mark all parents of this entity as having dirty children.
 * This guarantees the UpdateTransformTree traversal won't prune this branch.
 */
void MarkPathDirty(entt::registry& registry, entt::entity entity) {
    entt::entity curr = registry.get<TFRelationshipComponent>(entity).parent;
    while (curr != entt::null) {
        auto& rel = registry.get<TFRelationshipComponent>(curr);
        
        // Early exit: if this parent already knows, all parents above it do too.
        if (rel.has_dirty_children) break; 
        
        rel.has_dirty_children = true;
        curr = rel.parent;
    }
}

/**
 * @brief Adds the necessary Transform and Relationship components to an entity.
 */
void AddTransform(entt::registry& registry, entt::entity entity, 
                  const glm::vec3& position,
                  const glm::quat& rotation,
                  const glm::vec3& scale ) {
    TFRelationshipComponent rel_tf;
    TFLocalTransformComponent local_tf;
    TFWorldTransformComponent world_tf;


}

void AddTransform(entt::registry& registry, entt::entity child_entity, entt::entity parent_entity,
                  const glm::vec3& position,
                  const glm::quat& rotation,
                  const glm::vec3& scale ) {

}


void SetPosition(entt::registry& registry, entt::entity entity, const glm::vec3& pos) {
    registry.get<TFLocalTransformComponent>(entity).SetPosition(pos);
    MarkPathDirty(registry, entity);
}
void SetRotation(entt::registry& registry, entt::entity entity, const glm::quat& rot) {
    registry.get<TFLocalTransformComponent>(entity).SetRotation(rot);
    MarkPathDirty(registry, entity);
}
void SetScale(entt::registry& registry, entt::entity entity, const glm::vec3& scale) {
    registry.get<TFLocalTransformComponent>(entity).SetScale(scale);
    MarkPathDirty(registry, entity);
}

glm::vec3 GetLocalPosition(entt::registry& registry, entt::entity entity) {
    return registry.get<TFLocalTransformComponent>(entity).GetPosition();
}
glm::quat GetLocalRotation(entt::registry& registry, entt::entity entity) {
    return registry.get<TFLocalTransformComponent>(entity).GetPosition();
}
glm::vec3 GetLocalScale(entt::registry& registry, entt::entity entity) {
    return registry.get<TFLocalTransformComponent>(entity).GetScale();
}
const glm::mat4& GetWorldMatrix(entt::registry& registry, entt::entity entity) {
    return registry.get<TFWorldTransformComponent>(entity).matrix;
}



// --- TF Hierarchy ------------------------------------------------
/**
 * @brief Attaches a child to a parent with a specific dependency policy.
 */
void AttachChild(entt::registry& registry, entt::entity parent, entt::entity child, TFDependencyPolicy policy) {
    auto& parent_rel = registry.get<TFRelationshipComponent>(parent);
    auto& child_rel = registry.get<TFRelationshipComponent>(child);

    // Link to new parent
    child_rel.parent = parent;
    child_rel.dependency_policy = policy;
    child_rel.depth = parent_rel.depth + 1;
    
    // Insert at the front of the parent's children list
    child_rel.next_sibling = parent_rel.first_child;
    if (parent_rel.first_child != entt::null) {
        registry.get<TFRelationshipComponent>(parent_rel.first_child).prev_sibling = child;
    }
    parent_rel.first_child = child;

    // Since this child just moved into this branch, flag it so it inherits the parent's world matrix
    registry.get<TFLocalTransformComponent>(child).SetDirty();
    MarkPathDirty(registry, child);
}

/**
 * @brief Detaches a child from its parent, making it a root node in the World Coordinate System.
 */
void DetachChild(entt::registry& registry, entt::entity child);






// --- Safe Transform Setters ---

inline void SetLocalPosition(entt::registry& registry, entt::entity entity, const glm::vec3& pos) {
}

inline void SetLocalRotation(entt::registry& registry, entt::entity entity, const glm::quat& rot) {
    registry.get<TFLocalTransformComponent>(entity).SetRotation(rot);
    MarkPathDirty(registry, entity);
}

// --- Safe Hierarchy Management ---




}