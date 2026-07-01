# benchmarks/CMakeLists.txt
#
# Available Variables from Main CMakeLists.txt
# Base Flags:
#   RRL_TESTING, RRL_EXAMPLES, RRL_BENCHMARKS
#
# Platform Targets: 
#   RRL_DESKTOP_TARGET, RRL_TARGET_WASM
#
# Build Configurations:
#   RRL_BUILD_BACKEND_OPENGL   (Hardware OpenGL backend)
#   RRL_BUILD_BACKEND_WEBGPU   (Hardware WebGPU backend)
#   RRL_BUILD_WINDOW_OPENCV    (OpenCV presentation layer)
#
#

if (NOT RRL_BENCHMARKS) 
    return()
endif()

if (NOT RRL_DESKTOP_TARGET)
    message(WARNING "RRL [Benchmarks]: Benchmarks are currently only supported natively on Desktop targets. Skipping.")
    return()
endif()

# --- Enable Benchmarking (Fetch Google Benchmark) ----------------
message(STATUS "RRL [Benchmarks]: Fetching Google Benchmark framework...")
include(FetchContent)
FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable benchmark install" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

if(NOT TARGET benchmark::benchmark)
    message(FATAL_ERROR "RRL [Benchmarks]: Failed to fetch or configure the Google Benchmark framework.")
endif()


# --- Benchmark Dependencies --------------------------------------
find_package(Threads REQUIRED)

set(RRL_BENCHMARK_LIBS RRL::rrl benchmark::benchmark_main Threads::Threads)
set(RRL_BENCHMARK_INCLUDES )

if(RRL_BUILD_WINDOW_OPENCV AND OpenCV_FOUND)
    list(APPEND RRL_BENCHMARK_LIBS ${OpenCV_LIBS})
    list(APPEND RRL_BENCHMARK_INCLUDES ${OpenCV_INCLUDE_DIRS})
endif()


# --- Benchmark Executable Targets --------------------------------
# Collect all benchmark C++ source files directly in this folder and subfolders
file(GLOB_RECURSE RRL_BENCHMARK_SOURCES "*.cpp" "*.cc")

if(RRL_BENCHMARK_SOURCES)

    foreach(bench_file IN LISTS RRL_BENCHMARK_SOURCES)
        file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${bench_file})
        message(STATUS "RRL [Benchmarks]: Found benchmark source file: ${rel_path}")
        get_filename_component(bench_name ${bench_file} NAME_WE)
        set(target_name "bm_${bench_name}")
        
        add_executable(${target_name} ${bench_file})
        
        # Link all aggregated libraries and includes
        target_link_libraries(${target_name} PRIVATE ${RRL_BENCHMARK_LIBS})
        if(RRL_BENCHMARK_INCLUDES)
            target_include_directories(${target_name} PRIVATE ${RRL_BENCHMARK_INCLUDES})
        endif()
        
    endforeach()

else()
    message(WARNING "RRL [Benchmarks]: RRL_BENCHMARKS is ON, but no source files were found in the 'benchmarks/' directory.")
endif()