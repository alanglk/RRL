// RRL/include/modules/THIModule.hpp
#pragma once

#include <RRL/rrl_export.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "RRL/RRLTypes.hpp"
#include "RRL/tf/TFTypes.hpp"


namespace rrl {
    

//  RRL::Engine runtime context.
struct EngineContext;


class RRL_API TFModule {
public:
    // --- Constructor / Destructor ------------------------------------
    explicit TFModule(EngineContext* ctx);
    ~TFModule();


    // Rule of five (this class is owned by RRLEngine)
    TFModule(const TFModule&)               = delete;
    TFModule& operator=(const TFModule&)    = delete;
    TFModule(TFModule&&)                    = delete;
    TFModule& operator=(TFModule&&)         = delete;


    // --- TF Runtime --------------------------------------------------
    /**
     * @brief Register Tree Transform actions:
     *  - On TFRelationComponent destroy ->
     *          Update childrens in function of its TFDependencyPolicy 
     */
    // void RegisterTFActions(); // CALL IT ON CONSTRUCTION

    /**
     * @brief Ticks the Transform Tree. It iterates breadth-firstly starting at all 
     * root nodes and updates all the world transform matrices. It skips non changed branches.
     */
    void UpdateTransformTree();



    // --- TF Adding / Updating / Getting ------------------------------

    /**
     * @brief Adds a new transform to an entity as a world deatached TF
     */
    void AddTransform(ObjectID world_object, 
                      const glm::vec3& position = {0.0f, 0.0f, 0.0f}, 
                      const glm::quat& rotation = {1.0f, 0.0f, 0.0f, 0.0f}, 
                      const glm::vec3& scale = {1.0f, 1.0f, 1.0f});
    /**
     * @brief Adds a new transfrom to an entity and attachs it to another existing TF.
     * Keep in mind that the provided transform will be relative to the parent.
     * TF = PARENT -> CHILD
     */
    void AddTransform(ObjectID child_obj, ObjectID parent_obj,
                      const glm::vec3& position = {0.0f, 0.0f, 0.0f}, 
                      const glm::quat& rotation = {1.0f, 0.0f, 0.0f, 0.0f}, 
                      const glm::vec3& scale = {1.0f, 1.0f, 1.0f},
                      rrl::tf::TFDependencyPolicy policy = rrl::tf::TFDependencyPolicy::CASCADE_DELETE
                    );

    /**
     * @brief Sets / updates an already added transform position
     */
    void SetLocalPosition(ObjectID world_object, const glm::vec3& pos);

    /**
     * @brief Sets / updates an already added transform rotation
     */
    void SetLocalRotation(ObjectID world_object, const glm::quat& rot);

    /**
     * @brief Sets / updates an already added transform scale
     */
    void SetLocalScale(ObjectID world_object, const glm::vec3& scale);

    /**
     * @brief Gets the local transform position (ISO 8855 CS)
     */
    glm::vec3 GetLocalPosition(ObjectID world_object);

    /**
     * @brief Gets the local transform rotation (quaternion)
     */
    glm::quat GetLocalRotation(ObjectID world_object);

    /**
     * @brief Gets the local transform scale 
     */
    glm::vec3 GetLocalScale(ObjectID world_object);

    /**
     * @brief Retrieves the final, absolute 4x4 world matrix (ISO 8855 CS).
     */
    const glm::mat4& GetWorldMatrix(ObjectID world_object);



    // --- TF Hierarchy ------------------------------------------------
    /**
     * @brief Attaches a child to a parent with a specific dependency policy.
     */
    void AttachChild(ObjectID parent_object, ObjectID child_object, 
                     rrl::tf::TFDependencyPolicy policy = rrl::tf::TFDependencyPolicy::CASCADE_DELETE);

    /**
     * @brief Detaches a child from its parent, making it a root node in the World Coordinate System.
     */
    void DetachChild(ObjectID child_object);



private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl
