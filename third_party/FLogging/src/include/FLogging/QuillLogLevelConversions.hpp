// src/include/FLogging/QuillLogLevelConversions.hpp
#pragma once

#include "FLogging/FLogging.hpp"

#ifndef ENABLE_QUILL
    #error "This file (QuillLogLevelConversions.hpp) requires QUILL support."
#endif

namespace flogging {
    inline quill::LogLevel ToQuillLevel(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return quill::LogLevel::TraceL3;
            case LogLevel::Debug: return quill::LogLevel::Debug;
            case LogLevel::Info:  return quill::LogLevel::Info;
            case LogLevel::Warn:  return quill::LogLevel::Warning;
            case LogLevel::Error: return quill::LogLevel::Error;
            case LogLevel::Critical: return quill::LogLevel::Critical;
            default: return quill::LogLevel::Info;
        }
    }
    inline LogLevel FromQuillLevel(quill::LogLevel level) {
        switch (level) {
            case quill::LogLevel::TraceL3:  return LogLevel::Trace; 
            case quill::LogLevel::Debug:    return LogLevel::Debug; 
            case quill::LogLevel::Info:     return LogLevel::Info;  
            case quill::LogLevel::Warning:  return LogLevel::Warn;  
            case quill::LogLevel::Error:    return LogLevel::Error; 
            case quill::LogLevel::Critical: return LogLevel::Critical; 
            default: return LogLevel::Info;
        }
    }

}