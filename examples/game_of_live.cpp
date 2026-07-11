



#include <RRL/data/ImageData.hpp>
#include <array>
#include <entt/entity/fwd.hpp>
#include <random>
#include <thread>
#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>


// Visualization
#include <RRL/data/AssetManager.hpp>
#include <RRL/rhi/RHI.hpp>

// Logging
#include <FLogging/FLogging.hpp>



#define ASSERT(condition, msg) \
    if (!(condition)) { \
        LOG_ERROR(msg); \
        std::abort(); \
    }




enum class State : uint8_t {
    DEATH = 0,
    ALIVE = 1
};

class World {
    inline uint64_t Idx(uint32_t i, uint32_t j) const {
        ASSERT(i < width, "[WORLD] Idx(): i > world::width");
        ASSERT(j < height, "[WORLD] Idx(): i > world::height");
        return j * width + i;
    }
    // Max worl dimensions: 
    //      2**64 cells 
    std::vector<State> curr;
    std::vector<State> next;
public:
    const uint32_t width;
    const uint32_t height;
    const uint64_t world_size;
    
    World(uint32_t width, uint32_t height):
        width(width),height(height), world_size(width*height){
        curr.resize(world_size, State::DEATH);
        next = curr;
    }
    ~World() = default;
    World(const World&) = delete;
    World& operator=(const World& other) = delete;
    World(World&&) = delete;
    World& operator=(World&& other) = delete;
    


    State GetCurrState(uint32_t i, uint32_t j) const {
        return curr[Idx(i, j)];
    }
    State GetCurrState(uint64_t idx) const {
        ASSERT(idx < curr.size(), "[WORLD] GetCurrState(): idx >= curr::size()")
        return curr[idx];
    }
    void SetNextState(uint64_t idx, State state) {
        ASSERT(idx < next.size(), "[WORLD] SetNextState(): idx >= next::size()")
        next[idx] = state;
    }
    void SwapBuffers() {
        auto aux = std::move(curr);
        curr = std::move(next);
        next = std::move(aux);
    }
};


// Neighbors
const std::array<State, 8> GetCurrNeighborsStates(const World& world, uint64_t idx) {
    std::array<State, 8> neighbors;
    std::fill(neighbors.begin(), neighbors.end(), State::DEATH);
    uint32_t i = idx % world.width;
    uint32_t j = idx / world.width;

    // Available movements
    bool _i_m = i > 0;    bool _i_p = i < (world.width -1);
    bool _j_m = j > 0;    bool _j_p = j < (world.height -1);
    
    // Actual movements
    uint32_t i_m = i-1; uint32_t i_p = i+1;
    uint32_t j_m = j-1; uint32_t j_p = j+1;

    // Top-Left
    if ( _j_m && _i_m )     neighbors[0] = world.GetCurrState( i_m, j_m );
    // Top
    if ( _j_m )             neighbors[1] = world.GetCurrState( i, j_m );
    // Top-Right
    if ( _j_m && _i_p )     neighbors[2] = world.GetCurrState( i_p, j_m );

    // Mid-Left
    if ( _i_m )             neighbors[3] = world.GetCurrState( i_m, j);
    // Mid-Right
    if ( _i_p )             neighbors[4] = world.GetCurrState( i_p, j);


    // Bottom-Left
    if ( _j_p && _i_m)      neighbors[5] = world.GetCurrState( i_m, j_p );
    // Bottom
    if ( _j_p )             neighbors[6] = world.GetCurrState( i, j_p );
    // Bottom-Right
    if ( _j_p && _i_p)      neighbors[7] = world.GetCurrState( i_p, j_p );

    return neighbors;
}

void ApplyRulesToCell(World& world, uint64_t idx) {
    const auto neighbors = GetCurrNeighborsStates(world, idx);
    uint8_t alive_neighbors = 0;
    for (const auto& neigh : neighbors) {
        if (neigh == State::ALIVE) alive_neighbors++;
    }
    
    State curr_state = world.GetCurrState(idx);
    
    if (curr_state == State::ALIVE) {
        // If a cell is alive, it survives to the next generation 
        // if it has 2 or 3 live neighbors; otherwise it dies.
        world.SetNextState(idx, (alive_neighbors == 2 || alive_neighbors == 3) ? State::ALIVE : State::DEATH);
    } else {
        // If a cell is dead, it comes to life in the next generation 
        // if it has exactly 3 live neighbors; otherwise it stays dead
        world.SetNextState(idx, (alive_neighbors == 3) ? State::ALIVE : State::DEATH);
    }
}

// We just iterate every cell and swap the next-generation buffer
void TickWorld(World& world) {
    for (uint64_t idx = 0; idx < world.world_size; idx++) {
        ApplyRulesToCell(world, idx);
    }
    world.SwapBuffers();
}



// Visualization
class WorldViewer {
    entt::registry registry;
    rrl::rhi::RHIWindow main_window;
    entt::entity board_tex { entt::null };
    entt::entity ui_board_node { entt::null };

    inline rrl::data::ImageData GetBoardTemplate(uint32_t width, uint32_t height) const {
        return {
            .width          = width,
            .height         = height,
            .channels       = rrl::data::ImageChannelLayout::CH_4,
            .data_type      = rrl::data::ImageDataType::UINT8,
            .color_layout   = rrl::data::ImageColorLayout::RGB,
            .origin         = rrl::data::ImageOrigin::TOP_LEFT,
            .filter         = rrl::data::ImageFilter::NEAREST
        };
    }
    
public:
    const rrl::rhi::RHIWindow& GetMainWindow() const { return main_window; }
    bool Initialize(const World& world, uint32_t win_width, uint32_t win_height) {
        rrl::data::InitializeAssetManager(registry);

        main_window = rrl::rhi::CreateWindow(rrl::rhi::RHIWindowType::GLFW);
        
        // Window uses the display dimensions
        rrl::rhi::InitializeWindow(main_window, "Game of Life", win_width, win_height);
        
        if (!rrl::rhi::LoadBackend(rrl::rhi::RHIBackendType::OPENGL, registry)) {
            LOG_ERROR("[RRL Engine] Failed to load RHI backend!");
            return false;
        }
        if (!rrl::rhi::Initialize(registry, &main_window)) {
            LOG_ERROR("[RRL Engine] Failed to initialize RHI backend!");
            return false;
        }
        
        // Texture uses the simulation dimensions
        auto model = GetBoardTemplate(world.width, world.height);
        model.data.resize(world.world_size*4, 0x00);
        board_tex = rrl::data::CreateTexture(registry, "world_board", std::move(model));
        
        ui_board_node = registry.create();
        rrl::data::BindUITexture(registry, ui_board_node, board_tex);
        return true;
    }
    
    void Render(const World& world) {
        rrl::data::ImageData frame = GetBoardTemplate(world.width, world.height);
        frame.data.resize(world.world_size * 4, 0x00);
        
        for (uint64_t idx = 0; idx < world.world_size; idx++) {
            auto state = world.GetCurrState(idx);
            if (world.GetCurrState(idx) == State::ALIVE) {
                frame.data[idx * 4 + 0] = 0xFF; // Red
                frame.data[idx * 4 + 1] = 0xFF; // Green
                frame.data[idx * 4 + 2] = 0xFF; // Blue
                frame.data[idx * 4 + 3] = 0xFF; // Alpha
            } else {
                frame.data[idx * 4 + 3] = 0xFF; // Alpha
            }
        }
        rrl::data::UpdateTexture(registry, board_tex, std::move(frame));
        
        rrl::rhi::RenderFrame(registry);
        rrl::rhi::PollWindowEvents(main_window); 
    }
    
    void Shutdown() {
        rrl::data::DestroyAllAssets(registry);
        rrl::rhi::Shutdown(registry);
        rrl::rhi::DestroyWindow(registry, main_window);
    }

};




int main() {
    flogging::AddConsoleSink();
    flogging::InitLogger();
    
    
    uint32_t world_width    = 256; 
    uint32_t world_height   = 256;
    uint32_t window_width   = 1024;
    uint32_t window_height  = 1024;
    
    World world(world_width, world_height);
    auto gen = std::bind(std::uniform_int_distribution<>(0,1),std::default_random_engine());
    for (uint64_t idx = 0; idx < world.world_size; idx++) {
        State state = static_cast<State>( gen() );
        world.SetNextState(idx, state);
    }
    world.SwapBuffers();
    

    WorldViewer viewer;
    viewer.Initialize(world, window_width, window_height);

    // Main Loop
    while (true) {
        viewer.Render(world);
        
        TickWorld(world);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    viewer.Shutdown();
    flogging::ResetLogger();
}