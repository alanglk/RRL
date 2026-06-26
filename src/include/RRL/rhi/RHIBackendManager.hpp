// RRL/src/rhi/RHIBackendManager.hpp
#pragma once

#include "RRL/rhi/RHIBackend.hpp"

namespace rrl::rhi {

/**
 * @brief Internal engine singleton that holds the active RHI backend.
 */
class RHIBackendManager {
private:
    RHIBackend active_backend;
    RHIBackendManager() = default;

public:
    RHIBackendManager(const RHIBackendManager&) = delete;
    RHIBackendManager(RHIBackendManager&& other) = delete;
    RHIBackendManager& operator=(const RHIBackendManager&) = delete;
    RHIBackendManager& operator=(RHIBackendManager&&) = delete;
    ~RHIBackendManager() { Reset(); };

    /**
     * @brief Retrieves the global instance of the manager.
     */
    static RHIBackendManager& Instance() {
        static RHIBackendManager instance;
        return instance;
    }

    /**
     * @brief Gets a mutable reference to the active backend dispatch table.
     */
    RHIBackend& GetBackend() { 
        return active_backend; 
    }

    /**
     * @brief Replaces the current backend with a new one.
     */
    void SetBackend(const RHIBackend& backend) { 
        active_backend = backend; 
    }

    /**
     * @brief Clears the active backend.
     */
    void Reset() { 
        active_backend = RHIBackend{}; 
    }
};

} // namespace rrl::rhi