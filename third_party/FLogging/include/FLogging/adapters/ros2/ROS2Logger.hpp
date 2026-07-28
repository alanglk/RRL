/**
 * @file ROS2Logger.hpp
 * @author alangln (https://github.com/alanglk)
 * @brief Flexible Logging Library ROS2 adapter
 */
#pragma once

#include <FLogging/FLogging.hpp>
#include <rclcpp/rclcpp.hpp>

namespace flogging {

inline void AddROS2Sink(rclcpp::Logger ros_logger, LogLevel level = LogLevel::Info, LoggerType target = LoggerType::Normal) {
    AddCallbackSink([ros_logger](LogLevel msg_level, const std::string& msg){
        if (msg_level >= LogLevel::Error) {
            RCLCPP_ERROR(ros_logger, "%s", msg.c_str());
        } else if (msg_level == LogLevel::Warn) {
            RCLCPP_WARN(ros_logger, "%s", msg.c_str());
        } else if (msg_level == LogLevel::Info) {
            RCLCPP_INFO(ros_logger, "%s", msg.c_str());
        } else {
            RCLCPP_DEBUG(ros_logger, "%s", msg.c_str());
        }
    }, level, target);
}

}