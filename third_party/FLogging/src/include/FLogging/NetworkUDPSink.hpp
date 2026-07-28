// src/include/FLogging/NetworkUDPSink.hpp
#pragma once

#include "FLogging/FLogging.hpp"

#include <boost/asio.hpp>
#include <algorithm>
#include <iostream>
#include <vector>
#include <tuple>

#ifdef ENABLE_QUILL
#include <quill/sinks/Sink.h>
#endif

namespace flogging {

using boost::asio::ip::udp;

/**
 * @brief Handles all Boost.Asio UDP socket management and client subscriptions.
 * Inherits dynamically from whatever BaseInterface is passed to it.
 */
template <typename BaseInterface>
class BaseUDPSink : public BaseInterface {
protected:
    struct Subscriber {
        udp::endpoint endpoint;
        std::chrono::steady_clock::time_point last_seen;
    };

    boost::asio::io_context io_ctx; 
    udp::socket socket_;
    std::vector<Subscriber> subscribers;
    const std::chrono::seconds timeout_threshold{5};

    void process_subscriptions() {
        if (!socket_.is_open()) return;

        char buffer[64];
        udp::endpoint client_endpoint;
        boost::system::error_code ec;
        auto now = std::chrono::steady_clock::now(); 

        // Drain the incoming buffer and update timestamps
        while (true) {
            size_t bytes = socket_.receive_from(boost::asio::buffer(buffer), client_endpoint, 0, ec);

            if (!ec && bytes > 0) {
                auto it = std::find_if(subscribers.begin(), subscribers.end(), 
                    [&](const Subscriber& s) { return s.endpoint == client_endpoint; });

                if (it != subscribers.end()) {
                    it->last_seen = now; 
                } else {
                    subscribers.push_back({client_endpoint, now}); 
                }
            } else {
                break; 
            }
        }

        // Prune dead subscribers
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                [&](const Subscriber& s) { 
                    return (now - s.last_seen) > timeout_threshold; 
                }),
            subscribers.end()
        );
    }

public:
    explicit BaseUDPSink(int port) 
        : socket_(io_ctx, udp::endpoint(udp::v4(), port)) {
        
        std::cout << "[NetworkUDPSink] Server started on " << port << "\n";
        
        boost::system::error_code ec;
        std::ignore = socket_.non_blocking(true, ec);
        if (ec) {
            throw std::runtime_error("[BaseUDPSink] Failed to set UDP socket to non-blocking: " + ec.message());
        }
    }

    ~BaseUDPSink() override {
        if (socket_.is_open()) {
            boost::system::error_code ec;
            std::ignore = socket_.close(ec);
        }

        std::cout << "[NetworkUDPSink] Server stopped\n";
    }
};


struct NetworkUDPSinkStd final : public BaseUDPSink<flogging::ISink> {
public:
    flogging::LogLevel filter_level;

    NetworkUDPSinkStd(int port, flogging::LogLevel level) 
        : BaseUDPSink<flogging::ISink>(port), filter_level(level) {}

    void write(flogging::LogLevel msg_level, const std::string& msg) override {
        if (msg_level < filter_level) return;
        this->process_subscriptions();

        boost::system::error_code ec;
        for (auto const& sub : this->subscribers) {
            // Send message with newline appended since raw Boost.Asio doesn't add it
            std::string payload = msg + "\n";
            this->socket_.send_to(boost::asio::buffer(payload.data(), payload.size()), sub.endpoint, 0, ec);
        }
    }
};

#ifdef ENABLE_QUILL
class NetworkUDPSinkQuill final : public BaseUDPSink<quill::Sink> {
public:
    // Inherit the constructor from the templated base class
    using BaseUDPSink<quill::Sink>::BaseUDPSink;

    void write_log(quill::MacroMetadata const*, uint64_t,
                 std::string_view, std::string_view,
                 std::string const&, std::string_view,
                 quill::LogLevel, std::string_view,
                 std::string_view,
                 std::vector<std::pair<std::string, std::string>> const*,
                 std::string_view log_message, std::string_view) override {
                     
        this->process_subscriptions();

        boost::system::error_code ec;
        for (auto const& sub : this->subscribers) {
            this->socket_.send_to(boost::asio::buffer(log_message.data(), log_message.size()), sub.endpoint, 0, ec);
        }
    }

    void flush_sink() noexcept override {
        this->process_subscriptions();
    }
};
#endif

}