/**
 * @file ROS2Logger.hpp
 * @author alangln (https://github.com/alanglk)
 * @brief Flexible Logging Library RTMaps adapter
 */
#pragma once

#include "FLogging/FLogging.hpp"
#include <maps.hpp>

namespace flogging {

inline void AddRTMapsSink(LogLevel level = LogLevel::Info, LoggerType target = LoggerType::Normal) {
    AddCallbackSink([](LogLevel msg_level, const std::string& msg){
        if (msg_level >= LogLevel::Error) {
            MAPS::ReportError(msg.c_str());
        } else if (msg_level == LogLevel::Warn) {
            MAPS::ReportWarning(msg.c_str());
        } else {
            MAPS::ReportInfo(msg.c_str());
        }
    }, level, target);
}

}