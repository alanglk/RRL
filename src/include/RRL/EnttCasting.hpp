// RRL/src/include/RRL/EnttCasting.hpp
#pragma once

#include "RRL/RRLTypes.hpp"
#include <entt/entt.hpp>

namespace rrl {
    
/*
 * Inline helpers to cast public IDs into internal entt::entity handles.
*/

inline entt::entity ToEntt(ObjectID id) { return static_cast<entt::entity>(id); }
inline ObjectID ToObjectID(entt::entity e) { 
    return (e == entt::null) ? NULL_OBJECT : static_cast<ObjectID>(e); 
}

inline entt::entity ToEntt(AssetID id) { return static_cast<entt::entity>(id); }
inline AssetID ToAssetID(entt::entity e) { 
    return (e == entt::null) ? NULL_ASSET : static_cast<AssetID>(e); 
}

} // namespace rrl 
