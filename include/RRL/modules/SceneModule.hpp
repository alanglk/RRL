// RRL/include/modules/SceneModule.hpp
#pragma once

#include <RRL/rrl_export.h>


#include "RRL/RRLTypes.hpp"
#include "RRL/scene/SceneTypes.hpp"

#include <glm/glm.hpp>

namespace rrl {


//  RRL::Engine runtime context.
struct EngineContext;


class RRL_API SceneModule {
public:
    // --- Constructor / Destructor ------------------------------------
    explicit SceneModule(EngineContext* ctx);
    ~SceneModule();

    // Rule of five (this class is owned by RRLEngine)
    SceneModule(const SceneModule&)               = delete;
    SceneModule& operator=(const SceneModule&)    = delete;
    SceneModule(SceneModule&&)                    = delete;
    SceneModule& operator=(SceneModule&&)         = delete;



    // --- Basic Node Lifecycle ----------------------------------------
    /**
     * @brief Spawns a raw, empty world object.
     */
    ObjectID SpawnObject();
    /**
     * @brief Destroys a world object and cleans up its components.
     */
    void DestroyObject(ObjectID object);

    // void ClearScene();


    // --- Prefabs -----------------------------------------------------
    /**
     * @brief Instantiates a prefab or any nested sub-prefab using dot-notation.
     * Examples: 
     * SpawnPrefab("rungholt_city")
     * SpawnPrefab("rungholt_city.vehicle_1")
     * @return ObjectID The Root Entity, or NULL_OBJECT if the blueprint isn't cached.
     */
    ObjectID SpawnPrefab(const rrl::scene::PrefabID& blueprint_id);
    /**
     * @brief Destroys a spawned prefab.
     */
    void DestroyPrefab(ObjectID prefab_object, bool force_asset_deletion = false);


    // --- Scene Environment -------------------------------------------
    /**
     * @brief Set the scene environment color.
     */
    void SetEnvironmentColor(const glm::vec4& color);
    /**
     * @brief Set a scene cube map texture.
     */
    void SetEnvironmentCubemap(rrl::AssetID cubemap_asset);
    /**
     * @brief Set a scene spherical stiched texture.
     */
    void SetEnvironmentEquirectangular(rrl::AssetID texture_asset);
    /**
     * @brief Set a scene custom mesh-projected texture.
     */
    void SetEnvironmentCustomMesh(rrl::AssetID texture_asset, rrl::AssetID mesh_asset);


    // --- (Future) World Queries --------------------------------------
    // ObjectID FindObjectByName(const std::string& name);
    // std::vector<ObjectID> GetObjectsWithTag(const std::string& tag);


private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl