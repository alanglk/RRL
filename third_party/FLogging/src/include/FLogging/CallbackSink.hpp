// src/include/FLogging/CallbackSink.hpp
#pragma once

#include "FLogging/FLogging.hpp"
#include <string_view>

#ifdef ENABLE_QUILL
    #include <quill/sinks/Sink.h>
    #include "FLogging/QuillLogLevelConversions.hpp"
#endif


namespace flogging {


template <typename BaseInterface>
class BaseCallbackSink : public BaseInterface {
protected:
    flogging::LogCallback cb_;
public:
    explicit BaseCallbackSink(flogging::LogCallback cb) 
        : cb_(std::move(cb)) {}
    ~BaseCallbackSink() override = default;
};


struct CallbackSinkStd final : public BaseCallbackSink<flogging::ISink> {
public:
    flogging::LogLevel filter_level;
    CallbackSinkStd(flogging::LogCallback cb, flogging::LogLevel level) 
        : BaseCallbackSink<flogging::ISink>(std::move(cb)), filter_level(level) {}
        
    void write(flogging::LogLevel msg_level, const std::string& msg) override {
        if (msg_level < filter_level) return;
        if (this->cb_) {
            this->cb_(msg_level, msg);
        }
    }
};


#ifdef ENABLE_QUILL
class CallbackSinkQuill final : public BaseCallbackSink<quill::Sink> {
public:
    using BaseCallbackSink<quill::Sink>::BaseCallbackSink;

    void write_log(quill::MacroMetadata const*, uint64_t,
                 std::string_view, std::string_view,
                 std::string const&, std::string_view,
                 quill::LogLevel log_level, std::string_view,
                 std::string_view,
                 std::vector<std::pair<std::string, std::string>> const*,
                 std::string_view log_message, std::string_view) override {

        if (this->cb_) {
            this->cb_(flogging::FromQuillLevel(log_level), std::string(log_message));
        }
    }

    void flush_sink() noexcept override { }
};

#endif


}