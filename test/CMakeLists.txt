# tests/CMakeLists.txt
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


if (NOT RRL_TESTING) 
    return()
endif()
if (NOT RRL_DESKTOP_TARGET)
    message(WARNING "RRL [Tests]: Testing is currently only supported natively on Desktop targets. Skipping tests.")
    return()
endif()



# --- Enable Testing (Fetch GTest) --------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)            # To disable GMock building
set(GTEST_DONT_DEFINE_FOO 0 CACHE BOOL "" FORCE)    # To rename every GTest macro by adding the GTEST_* prefix
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # For Windows: Prevent overriding the parent project's compiler/linker settings
FetchContent_MakeAvailable(googletest)

if(NOT TARGET gtest)
    message(FATAL_ERROR "RRL [Tests]: Failed to fetch or configure the GoogleTest framework.")
endif()

enable_testing()



# --- Test Dependencies -------------------------------------------
set(RRL_TESTING_LIBS RRL::rrl GTest::gtest_main)
set(RRL_TESTING_INCLUDES )

if(RRL_BUILD_WINDOW_OPENCV AND OpenCV_FOUND)
    list(APPEND RRL_TESTING_LIBS ${OpenCV_LIBS})
    list(APPEND RRL_TESTING_INCLUDES ${OpenCV_INCLUDE_DIRS})
endif()

if(RRL_BUILD_BACKEND_OPENGL)
    # list(APPEND RRL_TESTING_LIBS OpenGL::GL glfw)
endif()



# --- Test Executable Targets -------------------------------------
file(GLOB_RECURSE RRL_TEST_SOURCES "*.cpp" "*.cc")
if(RRL_TEST_SOURCES)

    foreach(test_file IN LISTS RRL_TEST_SOURCES)
        file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})
        message(STATUS "RRL [Tests]: Found test source file: ${rel_path}")
    endforeach()

    add_executable(rrl_tests ${RRL_TEST_SOURCES})
    target_link_libraries(rrl_tests PRIVATE ${RRL_TESTING_LIBS})
    
    if(RRL_TESTING_INCLUDES)
        target_include_directories(rrl_tests PRIVATE ${RRL_TESTING_INCLUDES})
    endif()
    
    # Discover and add tests to CTest automatically
    include(GoogleTest)
    gtest_discover_tests(rrl_tests)
    
else()
    message(WARNING "RRL [Tests]: RRL_TESTING is ON, but no test source files were found in the 'tests/' directory.")
endif()


