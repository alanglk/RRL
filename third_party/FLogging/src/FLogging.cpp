/**
 * @file FLogging.cpp
 * @author alangln (https://github.com/alanglk)
 * @brief Implementation of the Flexible Logging Library (FLogging)
 * @version 0.1
 * @date 2026-06-11
 * @copyright Copyright (c) 2026
 */

#include "FLogging/FLogging.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <mutex>

#ifdef ENABLE_QUILL
    #include <quill/Backend.h>
    #include <quill/Frontend.h>
    #include <quill/sinks/ConsoleSink.h>
    #include <quill/sinks/RotatingFileSink.h>
#endif

namespace flogging {

    // --- Global State ------------------------------------------------
    BackendType g_active_backend = BackendType::StdFormat;
    LogLevel g_active_log_level = LogLevel::Info;
    LogLevel g_metrics_log_level = LogLevel::Info;
    std::mutex g_log_mutex;

    BackendType GetActiveBackend() {
        return g_active_backend;
    }

    // --- StdFormat Sinks ---------------------------------------------
    struct ISink {
        virtual ~ISink() = default;
        virtual void write(LogLevel msg_level, const std::string& msg) = 0;
    };

    struct ConsoleSinkStd : public ISink {
        LogLevel filter_level;
        ConsoleSinkStd(LogLevel level) : filter_level(level) {}
        void write(LogLevel msg_level, const std::string& msg) override {
            if (msg_level < filter_level) return;
            if (msg_level >= LogLevel::Error) std::cerr << msg << "\n";
            else std::cout << msg << "\n";
        }
    };

    struct FileSinkStd : public ISink {
        LogLevel filter_level;
        std::ofstream ofs;
        FileSinkStd(const std::string& path, LogLevel level) : filter_level(level), ofs(path, std::ios::app) {}
        void write(LogLevel msg_level, const std::string& msg) override {
            if (msg_level < filter_level) return;
            if (ofs.is_open()) ofs << msg << "\n";
        }
    };

    static std::vector<std::unique_ptr<ISink>> g_std_sinks;
    static std::vector<std::unique_ptr<ISink>> g_std_metrics_sinks;

} // namespace flogging



// --- Custom Sinks ------------------------------------------------
#include "FLogging/NetworkUDPSink.hpp" 
#include "FLogging/CallbackSink.hpp"

#ifdef ENABLE_QUILL
    #include "FLogging/QuillLogLevelConversions.hpp"
#endif



namespace flogging {

    // --- Quill -------------------------------------------------------
#ifdef ENABLE_QUILL
    static std::vector<std::shared_ptr<quill::Sink>> g_quill_sinks;
    static std::vector<std::shared_ptr<quill::Sink>> g_quill_metrics_sinks;

    quill::Logger* get_logger() {
        quill::Logger* logger = quill::Frontend::get_logger("LOGGER");
        if (!logger) std::cerr << "[FLogging] Error: Quill Logger not initialized!\n";
        return logger;
    }

    quill::Logger* get_metrics_logger() {
        quill::Logger* logger = quill::Frontend::get_logger("METRICS");
        if (!logger) std::cerr << "[FLogging] Error: Quill Metrics Logger not initialized!\n";
        return logger;
    }
#endif

    // --- API ---------------------------------------------------------
    void AddConsoleSink(LogLevel level, LoggerType target) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        
        // Register for StdFormat Fallback
        auto std_sink = std::make_unique<ConsoleSinkStd>(level);
        if (target == LoggerType::Metrics) g_std_metrics_sinks.push_back(std::move(std_sink));
        else g_std_sinks.push_back(std::move(std_sink));

#ifdef ENABLE_QUILL
        // Register for Quill
        std::string sink_name = "sink_stdout_" + std::to_string(static_cast<uint8_t>(target));
        auto q_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(sink_name);
        q_sink->set_log_level_filter(ToQuillLevel(level));

        if (target == LoggerType::Metrics) g_quill_metrics_sinks.push_back(q_sink);
        else g_quill_sinks.push_back(q_sink);
#endif
    }

    void AddFileSink(const std::string& file_path, LogLevel level, LoggerType target) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        
        // Register for StdFormat Fallback
        auto std_sink = std::make_unique<FileSinkStd>(file_path, level);
        if (target == LoggerType::Metrics) g_std_metrics_sinks.push_back(std::move(std_sink));
        else g_std_sinks.push_back(std::move(std_sink));

#ifdef ENABLE_QUILL
        // Register for Quill
        auto q_sink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(
            file_path,
            []() {
                quill::RotatingFileSinkConfig cfg;
                cfg.set_rotation_max_file_size(100 * 1024 * 1024); // 100MB
                cfg.set_max_backup_files(5);
                cfg.set_open_mode("w");
                return cfg;
            }()
        );
        q_sink->set_log_level_filter(ToQuillLevel(level));

        if (target == LoggerType::Metrics) g_quill_metrics_sinks.push_back(q_sink);
        else g_quill_sinks.push_back(q_sink);
#endif
    }

    void AddNetworkUDPSink(int port, LogLevel level, LoggerType target) {
        if (port <= 0) {
            std::cerr << "[FLogging] Error: Invalid port for Network UDP logging sink\n";
            return;
        }
        std::lock_guard<std::mutex> lock(g_log_mutex);
        
        // Register for StdFormat Fallback
        auto std_sink = std::make_unique<NetworkUDPSinkStd>(port, level);
        if (target == LoggerType::Metrics) g_std_metrics_sinks.push_back(std::move(std_sink));
        else g_std_sinks.push_back(std::move(std_sink));

#ifdef ENABLE_QUILL
        // Register for Quill
        std::string sink_name = "sink_udp_" + std::to_string(static_cast<uint8_t>(target)) + "_" + std::to_string(port);
        auto q_sink = quill::Frontend::create_or_get_sink<NetworkUDPSinkQuill>(sink_name, port);
        q_sink->set_log_level_filter(ToQuillLevel(level));

        if (target == LoggerType::Metrics) g_quill_metrics_sinks.push_back(q_sink);
        else g_quill_sinks.push_back(q_sink);
#endif
    }

    void AddCallbackSink(LogCallback callback, LogLevel level, LoggerType target) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        
        // Register for StdFormat Fallback
        auto std_sink = std::make_unique<CallbackSinkStd>(callback, level);
        if (target == LoggerType::Metrics) g_std_metrics_sinks.push_back(std::move(std_sink));
        else g_std_sinks.push_back(std::move(std_sink));

#ifdef ENABLE_QUILL
        // Register for Quill
        static uint32_t cb_counter = 0;
        std::string sink_name = "sink_callback_" + std::to_string(static_cast<uint8_t>(target)) + "_" + std::to_string(++cb_counter);
        auto q_sink = quill::Frontend::create_or_get_sink<CallbackSinkQuill>(sink_name, std::move(callback));
        q_sink->set_log_level_filter(ToQuillLevel(level));

        if (target == LoggerType::Metrics) g_quill_metrics_sinks.push_back(q_sink);
        else g_quill_sinks.push_back(q_sink);
#endif
    }



    void InitLogger(LogLevel overall_level, BackendType backend) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        g_active_log_level = overall_level;
        g_metrics_log_level = LogLevel::Info;

#ifdef ENABLE_QUILL
        if (backend == BackendType::Quill) {
            g_active_backend = BackendType::Quill;
            quill::BackendOptions backend_options;
            quill::Backend::start(backend_options);

            // Initialize Standard Logger
            if (!g_quill_sinks.empty()) {
                quill::PatternFormatterOptions opts {
                    "%(time) [%(thread_id)] %(short_source_location:<28) %(message)"
                };
                opts.add_metadata_to_multi_line_logs = false;
                auto logger = quill::Frontend::create_or_get_logger("LOGGER", g_quill_sinks, opts);
                logger->set_log_level(ToQuillLevel(overall_level));
                g_quill_sinks.clear();
            }

            // Initialize Metrics Logger
            if (!g_quill_metrics_sinks.empty()) {
                quill::PatternFormatterOptions metrics_opts {"%(time) %(message)"};
                metrics_opts.add_metadata_to_multi_line_logs = false;
                auto metrics_logger = quill::Frontend::create_or_get_logger("METRICS", g_quill_metrics_sinks, metrics_opts);
                metrics_logger->set_log_level(quill::LogLevel::Info);
                g_quill_metrics_sinks.clear();
            }
            return;
        }
#endif

        // Fallback
        if (backend == BackendType::Quill) {
            std::cerr << "[FLogging] Warning: Quill backend requested but FLogging was compiled without ENABLE_QUILL. Falling back to StdFormat.\n";
        }
        g_active_backend = BackendType::StdFormat;
    }

    void ResetLogger(LoggerType target) {
        std::lock_guard<std::mutex> lock(g_log_mutex);

        if (target == LoggerType::None || target == LoggerType::Normal) {
            g_std_sinks.clear();
        }
        if (target == LoggerType::None || target == LoggerType::Metrics) {
            g_std_metrics_sinks.clear();
        }

#ifdef ENABLE_QUILL
        if (g_active_backend == BackendType::Quill) {
            quill::Logger* std_logger = quill::Frontend::get_logger("LOGGER");
            quill::Logger* metrics_logger = quill::Frontend::get_logger("METRICS");

            if ((target == LoggerType::None || target == LoggerType::Normal) && std_logger) {
                quill::Frontend::remove_logger(std_logger);
            }
            if ((target == LoggerType::None || target == LoggerType::Metrics) && metrics_logger) {
                quill::Frontend::remove_logger(metrics_logger);
            }
            if (target == LoggerType::None) {
                quill::Backend::stop();
            }
            g_quill_sinks.clear();
            g_quill_metrics_sinks.clear();
        }
#endif
    }

    LogLevel GetLogLevel(LoggerType target) {
#ifdef ENABLE_QUILL
        if (g_active_backend == BackendType::Quill) {
            std::string logger_name = (target == LoggerType::Metrics) ? "METRICS" : "LOGGER";
            if (quill::Logger* logger = quill::Frontend::get_logger(logger_name)) {
                return FromQuillLevel(logger->get_log_level());
            }
            return LogLevel::Info;
        }
#endif
        return (target == LoggerType::Metrics) ? g_metrics_log_level : g_active_log_level;
    }

    void SetLogLevel(LogLevel level, LoggerType target) {
#ifdef ENABLE_QUILL
        if (g_active_backend == BackendType::Quill) {
            std::string logger_name = (target == LoggerType::Metrics) ? "METRICS" : "LOGGER";
            if (quill::Logger* logger = quill::Frontend::get_logger(logger_name)) {
                logger->set_log_level(ToQuillLevel(level));
            }
        }
#endif
        if (target == LoggerType::Metrics) g_metrics_log_level = level;
        else g_active_log_level = level;
    }

    // StdFormat backend dispatchers
    void DispatchLog(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        for (auto& sink : g_std_sinks) {
            sink->write(level, message);
        }
    }
    void DispatchMetric(const std::string& message) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (g_std_metrics_sinks.empty()) {
            std::cout << message << "\n"; // Default fallback
        } else {
            for (auto& sink : g_std_metrics_sinks) {
                sink->write(LogLevel::Info, message);
            }
        }
    }

} // namespace flogging