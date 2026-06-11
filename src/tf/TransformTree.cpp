// RRL/src/tf/TransformTree.cpp

#include "RRL/tf/TransformTree.hpp"
#include "RRL/tf/TFComponents.hpp"
#include "entt/entity/fwd.hpp"

#include <glm/gtx/matrix_decompose.hpp>

#include <FLogging/FLogging.hpp>
#include "RRL/DebugMacros.hpp"


namespace rrl::tf {
    


// --- TF Runtime --------------------------------------------------

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
        auto* child_rel = registry.try_get<TFRelationshipComponent>(current_child);
        if (!child_rel) break;

        entt::entity next_sibling = child_rel->next_sibling; 
        TFDependencyPolicy policy = child_rel->dependency_policy;

        // Cascade deletion
        if (policy == TFDependencyPolicy::CASCADE_DELETE) {
            registry.destroy(current_child);
        }

        // Reparent this node to WORLD
        else if (policy == TFDependencyPolicy::REPARENT_TO_WORLD) {

            // The parent is actively being destroyed from RAM, so climbing the tree is impossible.
            // We MUST use the child's cached world matrix as the last known accurate state.
            if (auto* world = registry.try_get<TFWorldTransformComponent>(current_child)) {
                if (auto* local = registry.try_get<TFLocalTransformComponent>(current_child)) {
                    // This sets is_dirty = true, flagging it for the next UpdateTransformTree call
                    local->SetFromMatrix(world->matrix); 
                }
            }
            
            // Now it becomes a root node
            child_rel->parent = entt::null;
            child_rel->prev_sibling = entt::null;
            child_rel->next_sibling = entt::null;
            child_rel->depth = 0;
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
    if (!registry.all_of<TFRelationshipComponent>(entity)) return;

    entt::entity curr = registry.get<TFRelationshipComponent>(entity).parent;
    while (curr != entt::null) {
        auto& rel = registry.get<TFRelationshipComponent>(curr);
        
        // Early exit: if this parent already knows, all parents above it do too.
        if (rel.has_dirty_children) break; 
        
        rel.has_dirty_children = true;
        curr = rel.parent;
    }
}

void AddTransform(entt::registry& registry, entt::entity entity, 
                  const glm::vec3& position,
                  const glm::quat& rotation,
                  const glm::vec3& scale ) {

    if (registry.all_of<TFLocalTransformComponent>(entity)) {
        LOG_ERROR("Entity {} already has a transform. Cannot add multiple transforms.", static_cast<uint32_t>(entity));
        return;
    }

    TFLocalTransformComponent local_tf;
    local_tf.SetPosition(position);
    local_tf.SetRotation(rotation);
    local_tf.SetScale(scale);

    registry.emplace<TFLocalTransformComponent>(entity, local_tf);
    registry.emplace<TFWorldTransformComponent>(entity);
    registry.emplace<TFRelationshipComponent>(entity);
}

void AddTransform(entt::registry& registry, entt::entity child_entity, entt::entity parent_entity,
                  const glm::vec3& position,
                  const glm::quat& rotation,
                  const glm::vec3& scale,
                  TFDependencyPolicy policy ) {

    AddTransform(registry, child_entity, position, rotation, scale);
    AttachChild(registry, parent_entity, child_entity, policy);
}


void SetLocalPosition(entt::registry& registry, entt::entity entity, const glm::vec3& pos) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFLocalTransformComponent, "Entity lacks a transform!");

    registry.get<TFLocalTransformComponent>(entity).SetPosition(pos);
    MarkPathDirty(registry, entity);
}
void SetLocalRotation(entt::registry& registry, entt::entity entity, const glm::quat& rot) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFLocalTransformComponent, "Entity lacks a transform!");

    registry.get<TFLocalTransformComponent>(entity).SetRotation(rot);
    MarkPathDirty(registry, entity);
}
void SetLocalScale(entt::registry& registry, entt::entity entity, const glm::vec3& scale) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFLocalTransformComponent, "Entity lacks a transform!");

    registry.get<TFLocalTransformComponent>(entity).SetScale(scale);
    MarkPathDirty(registry, entity);
}

glm::vec3 GetLocalPosition(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFLocalTransformComponent, "Entity lacks a transform!");

    return registry.get<TFLocalTransformComponent>(entity).GetPosition();
}
glm::quat GetLocalRotation(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFLocalTransformComponent, "Entity lacks a transform!");

    return registry.get<TFLocalTransformComponent>(entity).GetRotation();
}
glm::vec3 GetLocalScale(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFLocalTransformComponent, "Entity lacks a transform!");

    return registry.get<TFLocalTransformComponent>(entity).GetScale();
}
const glm::mat4& GetWorldMatrix(entt::registry& registry, entt::entity entity) {
    RRL_ASSERT_HAS_COMPONENT(registry, entity, TFWorldTransformComponent, "Entity lacks a transform!");

    return registry.get<TFWorldTransformComponent>(entity).matrix;
}



// --- TF Hierarchy ------------------------------------------------
void AttachChild(entt::registry& registry, entt::entity parent, entt::entity child, TFDependencyPolicy policy) {
    if (!registry.all_of<TFRelationshipComponent>(parent) || !registry.all_of<TFRelationshipComponent>(child)) {
        LOG_ERROR("AttachChild failed. Both parent {} and child {} must have transforms.", static_cast<uint32_t>(parent), static_cast<uint32_t>(child));
        return;
    }
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
        RRL_ASSERT_HAS_COMPONENT(reg, node, TFLocalTransformComponent, "Entity lacks a transform!");
        const auto& local = reg.get<TFLocalTransformComponent>(curr);
        abs_matrix = local.GetLocalMatrix() * abs_matrix; 
        curr = reg.get<TFRelationshipComponent>(curr).parent;
    }
    return abs_matrix;
}
void DetachChild(entt::registry& registry, entt::entity child) {
    if (!registry.all_of<TFRelationshipComponent>(child)) {
        LOG_ERROR("DetachChild failed. Entity {} lacks a relationship component.", static_cast<uint32_t>(child));
        return;
    }

    auto& child_rel = registry.get<TFRelationshipComponent>(child);
    if (child_rel.parent == entt::null) {
        LOG_WARN("Entity {} is already a root node. Nothing to detach.", static_cast<uint32_t>(child));
        return;
    }

    // Unlink from siblings and parent
    if (child_rel.prev_sibling != entt::null) {
        registry.get<TFRelationshipComponent>(child_rel.prev_sibling).next_sibling = child_rel.next_sibling;
    } else if (child_rel.parent != entt::null) {
        registry.get<TFRelationshipComponent>(child_rel.parent).first_child = child_rel.next_sibling;
    }
    if (child_rel.next_sibling != entt::null) {
        registry.get<TFRelationshipComponent>(child_rel.next_sibling).prev_sibling = child_rel.prev_sibling;
    }

    // Convert current absolute coordinates to be its new root-level coordinates
    glm::mat4 true_world_matrix = ComputeAbsoluteMatrix(registry, child);
    if (registry.all_of<TFLocalTransformComponent>(child)) {
        registry.get<TFLocalTransformComponent>(child).SetFromMatrix(true_world_matrix);
    }

    // Clear relationships
    child_rel.parent = entt::null;
    child_rel.prev_sibling = entt::null;
    child_rel.next_sibling = entt::null;
    child_rel.depth = 0;
    
    MarkPathDirty(registry, child);
}



}

