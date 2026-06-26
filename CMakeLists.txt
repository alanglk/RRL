# ==============================================================================
# @Project:     Robotics Rendering Library (RRL)
# @Author:      https://github.com/alanglk
# @Date:        10/06/2026
# @Description: CMake configuration for the Robotics Rendering Library (RRL) project.
# @Version:     1.0.0
# ==============================================================================

cmake_minimum_required(VERSION 3.15)

project(RRL 
    VERSION 1.0.0 
    DESCRIPTION "Robotics Rendering Library (RRL)"
    HOMEPAGE_URL "https://github.com/alanglk/RRL"
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(RRL_IS_MASTER_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(RRL_IS_MASTER_PROJECT ON)
endif()


# --- Compilation out paths ---------------------------------------
if(RRL_IS_MASTER_PROJECT)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
endif()


# --- Building options --------------------------------------------
option(RRL_SHARED   "Build RRL as a shared library" OFF )
option(RRL_TESTING  "Build RRL tests" OFF )
option(RRL_EXAMPLES "Build RRL examples" OFF)
option(RRL_BUILD_BACKEND_OPENCV "Build the OpenCV Rasterizer RHI Backend" ON)


# --- Installation Variables --------------------------------------
if(RRL_IS_MASTER_PROJECT)
    # Install tree inside the build dir by default <build>/dist
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
      set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist" CACHE PATH "Install path" FORCE)
    endif()
endif()

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN" "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(CMAKE_SKIP_RPATH FALSE)
set(CMAKE_MACOSX_RPATH FALSE)


# --- Library Setup Variables -------------------------------------
set(RRL_TYPE STATIC)                # STATIC or SHARED
set(RRL_PUBLIC_LIBRARIES )          # Public RRL dependencies (entt, glm)
set(RRL_PRIVATE_LIBRARIES )         # Private RRL dependencies (no deps for now)
set(RRL_PUBLIC_COMPILE_DEFS )       # e.g. ENABLE_CUDA to compile CUDA RHI and tell lib linkers it is enabled
set(RRL_PRIVATE_COMPILE_DEFS )      # e.g. ENABLE_TESTING compile definition to compile tests source sections

if(RRL_SHARED)
    set(RRL_TYPE SHARED)
endif()


# --- Library Sources ---------------------------------------------
set(RRL_SOURCES
    src/tf/TransformTree.cpp
    src/tf/TFComponents.cpp

    src/camera/CameraSystem.cpp

    src/io/PrefabIO.cpp
    src/io/ImageIO.cpp

    src/data/AssetManager.cpp
    src/data/SynchronizationSystems.cpp

    src/scene/SceneManager.cpp

    src/rhi/RHIAPI.cpp
    
    src/debug/TFDebugger.cpp
    src/debug/AssetManagerDebugger.cpp
    src/debug/SceneManagerDebugger.cpp

)



# --- Enable Testing (Fetch GTest) --------------------------------
if(RRL_TESTING)
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
    
    enable_testing()

    list(APPEND RRL_PRIVATE_COMPILE_DEFS ENABLE_TESTING)
endif()


# --- Library Dependencies ----------------------------------------

## EnTT: located in third_party/entt 
set(ENTT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/entt")
if(EXISTS "${ENTT_DIR}/CMakeLists.txt")
    set(ENTT_INSTALL OFF CACHE BOOL "" FORCE) # Prevents EnTT overrides on install variables
    add_subdirectory(${ENTT_DIR} "${CMAKE_CURRENT_BINARY_DIR}/entt" EXCLUDE_FROM_ALL)
    list(APPEND RRL_PUBLIC_LIBRARIES $<BUILD_INTERFACE:EnTT::EnTT> )
else()
    message(FATAL_ERROR "EnTT source missing at ${ENTT_DIR}. Have you called 'git submodule update --init --recursive'?")
endif()

## GLM: located in third_party/glm 
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")
if(EXISTS "${GLM_DIR}/CMakeLists.txt")
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GLM_DIR} "${CMAKE_CURRENT_BINARY_DIR}/glm" EXCLUDE_FROM_ALL)
    list(APPEND RRL_PUBLIC_LIBRARIES $<BUILD_INTERFACE:glm::glm-header-only> )
    list(APPEND RRL_PRIVATE_COMPILE_DEFS GLM_ENABLE_EXPERIMENTAL)
else()
    message(FATAL_ERROR "GLM source missing at ${GLM_DIR}. Have you called 'git submodule update --init --recursive'?")
endif()

## FLogging: located in third_party/FLogging 
set(FL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/FLogging")
if(EXISTS "${FL_DIR}/CMakeLists.txt")
    if (RRL_TYPE STREQUAL "SHARED")
        set(FLOGGING_SHARED ON CACHE BOOL "Force FLogging as SHARED" FORCE)
    endif()
    set(FLOGGING_F_PREFIX OFF CACHE BOOL "Disable F_ prefix for parent project" FORCE)
    set(FLOGGING_QUILL_BACKEND ON CACHE BOOL "Force Quill backend for FLogging" FORCE)
    add_subdirectory(${FL_DIR} "${CMAKE_CURRENT_BINARY_DIR}/FLogging" EXCLUDE_FROM_ALL)
    list(APPEND RRL_PUBLIC_LIBRARIES FLogging::flogging)
else()
    message(FATAL_ERROR "FLogging source missing at ${FL_DIR}. Have you called 'git submodule update --init --recursive'?")
endif()

## OpenCV RHI Backend
if(RRL_BUILD_BACKEND_OPENCV)
    find_package(OpenCV QUIET)
    if(OpenCV_FOUND)
        message(STATUS "RRL: OpenCV found. Enabling OpenCV RHI backend.")
        list(APPEND RRL_PRIVATE_LIBRARIES ${OpenCV_LIBS})
        list(APPEND RRL_PRIVATE_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
        list(APPEND RRL_PUBLIC_COMPILE_DEFS RRL_RHI_OPENCV)
        list(APPEND RRL_SOURCES 
            src/rhi/opencv/OpenCVRenderer.cpp
        )
    else()
        message(WARNING "RRL: OpenCV not found! Disabling the OpenCV RHI backend despite RRL_BUILD_BACKEND_OPENCV being ON.")
        set(RRL_BUILD_BACKEND_OPENCV OFF CACHE BOOL "Build the OpenCV Rasterizer RHI Backend" FORCE)
    endif()
endif()


# --- Library Target ----------------------------------------------
add_library(rrl ${RRL_TYPE} ${RRL_SOURCES})
set_target_properties(rrl PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(RRL_SHARED)
    set_target_properties(rrl PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
endif()

include(GenerateExportHeader)
generate_export_header(rrl
    EXPORT_MACRO_NAME RRL_API
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/RRL/rrl_export.h"
)

target_include_directories(rrl 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader
)
target_compile_definitions(rrl 
    PUBLIC  ${RRL_PUBLIC_COMPILE_DEFS}
    PRIVATE ${RRL_PRIVATE_COMPILE_DEFS}
)
target_link_libraries(rrl 
    PUBLIC  ${RRL_PUBLIC_LIBRARIES}
    PRIVATE ${RRL_PRIVATE_LIBRARIES}
)
add_library(RRL::rrl ALIAS rrl)


# --- Test Executable Targets -------------------------------------
if(RRL_TESTING)
    # collect all test C++ source files in the test/ folder
    file(GLOB_RECURSE RRL_TEST_SOURCES "test/*.cpp" "test/*.cc")
    if(RRL_TEST_SOURCES)

        foreach(test_file IN LISTS RRL_TEST_SOURCES)
            file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})
            message(STATUS "Found test source file: ${rel_path}")
        endforeach()

        add_executable(rrl_tests ${RRL_TEST_SOURCES})
        target_link_libraries(rrl_tests 
            PRIVATE 
                rrl 
                GTest::gtest_main
        )
        
        # Discover and add tests to ctest automatically
        include(GoogleTest)
        gtest_discover_tests(rrl_tests)
    else()
        message(WARNING "Testing is enabled, but no test source files were found in 'test/'.")
    endif()
endif()

# --- Examples ----------------------------------------------------
if(RRL_EXAMPLES)
    # Collect all example C++ source files in the examples/ folder
    file(GLOB_RECURSE RRL_EXAMPLE_SOURCES "examples/*.cpp" "examples/*.cc")
    if(RRL_EXAMPLE_SOURCES)

        # Temp linking for examples
        find_package(Threads REQUIRED)
        find_package(Boost REQUIRED)
        find_package(OpenCV REQUIRED)

        foreach(example_file IN LISTS RRL_EXAMPLE_SOURCES)
            file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${example_file})
            message(STATUS "Found example source file: ${rel_path}")
            get_filename_component(example_name ${example_file} NAME_WE)
            set(target_name "ex_${example_name}")
            add_executable(${target_name} ${example_file})
            target_link_libraries(${target_name} 
                PRIVATE 
                    rrl 

                    Threads::Threads
                    Boost::boost
                    ${OpenCV_LIBS}
            )
        endforeach()

    else()
        message(WARNING "Examples building is enabled, but no example source files were found in 'examples/'.")
    endif()
endif()


# --- Install -----------------------------------------------------
install(
  TARGETS rrl
  EXPORT RRLTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION include
)
install(
  DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
  DESTINATION include
)

# Export CMake targets
install(
  EXPORT RRLTargets 
  FILE  RRLTargets.cmake
  NAMESPACE  RRL::
  DESTINATION lib/cmake/RRL
)

# Generate Config file
include(CMakePackageConfigHelpers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/RRLConfig.cmake "
include(CMakeFindDependencyMacro)
find_dependency(Threads)
include(\"\${CMAKE_CURRENT_LIST_DIR}/RRLTargets.cmake\")
")
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/RRLConfig.cmake
    DESTINATION lib/cmake/RRL
)


