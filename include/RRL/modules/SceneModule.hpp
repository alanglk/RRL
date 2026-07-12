// RRL/include/modules/SceneModule.hpp
#pragma once

#include <RRL/rrl_export.h>


#include "RRL/RRLTypes.hpp"
#include "RRL/scene/SceneTypes.hpp"
#include "RRL/io/PrefabIO.hpp"


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



    // --- Prefabs -----------------------------------------------------
    /**
     * @brief Takes ownership of raw prefab data, uploads assets to VRAM, 
     * and caches the lightweight blueprint.
     */
    void PreloadPrefabBlueprint(const rrl::scene::PrefabID& blueprint_id, rrl::io::IOPrefab&& prefab_data);

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



    // --- (Future) Scene Management -----------------------------------
    // void ClearScene();
    // void DontDestroyOnLoad(ObjectID object);

    // --- (Future) Global Environment ---------------------------------
    // void SetSkybox(AssetID cubemap_texture);
    // void SetAmbientLight(const glm::vec3& color, float intensity);
    // void SetDirectionalLight(const glm::vec3& direction, const glm::vec3& color);

    // --- (Future) World Queries --------------------------------------
    // ObjectID FindObjectByName(const std::string& name);
    // std::vector<ObjectID> GetObjectsWithTag(const std::string& tag);


private:
    EngineContext* m_ctx { nullptr };
};


} // namespace rrl