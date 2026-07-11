#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>

#include <FLogging/FLogging.hpp>
#include <boost/lockfree/queue.hpp>

// RRL Engine Modules
#include "RRL/data/AssetManager.hpp"
#include "RRL/data/ImageData.hpp"
#include "RRL/camera/CameraSystem.hpp"
#include "RRL/rhi/RHI.hpp"

using namespace rrl;

// --- Performance Metrics Configuration ---
constexpr size_t NUM_STREAMS = 8;         
constexpr uint32_t STREAM_W = 320;        
constexpr uint32_t STREAM_H = 240;        
constexpr size_t QUEUE_CAPACITY = 4;      

static std::atomic<bool> g_test_running{ true };
static std::atomic<uint64_t> g_total_frames_decoded{ 0 };

struct StreamChannel {
    // Boost lockfree queue requires continuous trivial pointer layouts
    boost::lockfree::queue<data::ImageData*, boost::lockfree::capacity<QUEUE_CAPACITY>> queue;
};
static std::vector<std::unique_ptr<StreamChannel>> g_stream_corridors;

// --- Producer: High-Frequency Synthetic Stream Workers ---
void SyntheticFrameGeneratorWorker(size_t stream_index) {
    std::mt19937 rng(static_cast<unsigned int>(stream_index));
    std::uniform_int_distribution<int> color_dist(0, 255);
    uint8_t base_g = static_cast<uint8_t>(color_dist(rng));
    
    uint64_t frame_counter = 0;
    const size_t payload_bytes = STREAM_W * STREAM_H * 3;

    while (g_test_running.load(std::memory_order_acquire)) {
        auto* frame = new data::ImageData();
        frame->width = STREAM_W;
        frame->height = STREAM_H;
        frame->channels = data::ImageChannelLayout::CH_3;
        frame->data_type = data::ImageDataType::UINT8;
        frame->color_layout = data::ImageColorLayout::BGR;
        frame->data.resize(payload_bytes, 0);

        uint8_t dynamic_wave = static_cast<uint8_t>((std::sin(frame_counter++ * 0.1f) * 0.5f + 0.5f) * 255.0f);
        
        // Populate local memory structure
        for (size_t i = 0; i < payload_bytes; i += 3) {
            frame->data[i]     = dynamic_wave;                       // B
            frame->data[i + 1] = base_g;                             // G
            frame->data[i + 2] = static_cast<uint8_t>(i % 256);      // R
        }

        if (g_stream_corridors[stream_index]->queue.push(frame)) {
            g_total_frames_decoded.fetch_add(1, std::memory_order_relaxed);
        } else {
            delete frame; // Secure memory drop if queue overrun occurs
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // ~125Hz generation
    }
}

int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger(flogging::LogLevel::Warn, flogging::BackendType::StdFormat);
    entt::registry registry;

    uint32_t window_w       = 1280;
    uint32_t window_h       = 960;

    data::InitializeAssetManager(registry);
    rrl::rhi::RHIWindow main_window = rrl::rhi::CreateWindow(rrl::rhi::RHIWindowType::OPENCV);
    rrl::rhi::InitializeWindow(main_window, "RRL - Multi-Stream Hardened Test", window_w, window_h);
    if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::SOFTWARE, registry)) {
        LOG_ERROR("[RRL Engine] Failed to load RHI backend!");
        return -1;
    }
    if (!rrl::rhi::Initialize(registry, &main_window)) {
        LOG_ERROR("[RRL Engine] Failed to initialize RHI backend!");
        return -1;
    }

    std::vector<entt::entity> ui_textures(NUM_STREAMS);
    g_stream_corridors.reserve(NUM_STREAMS);

    float columns = 4.0f;
    float rows = 2.0f;
    float element_w = 1.0f / columns;
    float element_h = 1.0f / rows;

    for (size_t i = 0; i < NUM_STREAMS; ++i) {
        g_stream_corridors.push_back(std::make_unique<StreamChannel>());

        // CRITICAL: Ensure base allocations strictly mirror consumer data layout size expectations
        data::ImageData base_alloc;
        base_alloc.width = STREAM_W;
        base_alloc.height = STREAM_H;
        base_alloc.channels = data::ImageChannelLayout::CH_3;
        base_alloc.data_type = data::ImageDataType::UINT8;
        base_alloc.color_layout = data::ImageColorLayout::BGR;
        base_alloc.data.resize(STREAM_W * STREAM_H * 3, 0);
        
        std::string ui_texture_id = "ui_texture_" + std::to_string(i);
        ui_textures[i] = data::CreateTexture(registry, ui_texture_id, std::move(base_alloc));

        float x_pos = (i % 4) * element_w;
        float y_pos = (i / 4) * element_h;

        entt::entity tile_ui = registry.create();
        data::BindUITexture(registry, tile_ui, ui_textures[i], x_pos, y_pos, element_w - 0.01f, element_h - 0.01f);
    }

    // Launch worker execution group
    std::vector<std::thread> worker_pool;
    worker_pool.reserve(NUM_STREAMS);
    for (size_t i = 0; i < NUM_STREAMS; ++i) {
        worker_pool.emplace_back(SyntheticFrameGeneratorWorker, i);
    }

    auto start_perf_time = std::chrono::high_resolution_clock::now();
    uint64_t last_consumed_count = 0;
    uint64_t total_frames_consumed = 0;

    // Main Engine Consuming & Processing Loop
    while (true) {
        for (size_t i = 0; i < NUM_STREAMS; ++i) {
            data::ImageData* frame_ptr = nullptr;
            
            // Drain queue entirely to capture the absolute newest frame
            while (g_stream_corridors[i]->queue.pop(frame_ptr)) {
                if (frame_ptr != nullptr) {
                    // Safe verification block before applying move operations
                    if (!frame_ptr->data.empty()) {
                        data::UpdateTexture(registry, ui_textures[i], std::move(*frame_ptr));
                        total_frames_consumed++;
                    }
                    delete frame_ptr; // Free outer shell container cleanly
                    frame_ptr = nullptr;
                }
            }
        }

        rrl::rhi::RenderFrame(registry);
        rrl::rhi::PollWindowEvents(main_window); 

        // Metrics output loop
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_perf_time).count();
        if (duration >= 2000) {
            double seconds = duration / 1000.0;
            uint64_t current_consumed = total_frames_consumed;
            uint64_t diff = current_consumed - last_consumed_count;
            double fps_throughput = static_cast<double>(diff) / seconds;

            LOG_WARN("[PERF METRICS] Throughput: {:.2f} Total Frames/Sec across all channels", fps_throughput);

            last_consumed_count = current_consumed;
            start_perf_time = current_time;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Keep main thread bounded to ~60Hz display refresh
    }

    // Cleanup Sequence
    g_test_running.store(false, std::memory_order_release);
    for (auto& thread : worker_pool) {
        if (thread.joinable()) thread.join();
    }

    flogging::ResetLogger();
    return 0;
}