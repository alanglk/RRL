// RRL/include/debug/RHIDebugger.hpp
#pragma once

#include <RRL/rrl_export.h>

#include "RRL/rhi/RHITypes.hpp"


namespace rrl::debug {


// --- Debug Structs -----------------------------------------------
struct RRL_API RHIDebugReport {
    rrl::rhi::RHIDebugFlag active_flag { rrl::rhi::RHIDebugFlag::FLAG_NONE };
};



} // namespace rrl::debug