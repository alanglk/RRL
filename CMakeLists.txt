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
option(RRL_SHARED                   "Build RRL as a shared library" OFF )
option(RRL_TESTING                  "Build RRL tests" OFF )
option(RRL_EXAMPLES                 "Build RRL examples" OFF)
option(RRL_BENCHMARKS               "Build RRL performance benchmarks" OFF)
option(RRL_ENABLE_NATIVE_ARCH       "Optimize for the host CPU architecture (breaks portability)" OFF)
option(RRL_BUILD_SWR_SIMD_BACKEND   "Build the software rendering backend with SIMD acceleration" ON)

# RHI Backend Toggles
option(RRL_BUILD_BACKEND_OPENGL     "Build the hardware-accelerated OpenGL backend" OFF)
option(RRL_BUILD_BACKEND_WEBGPU     "Build the WebGPU backend for WASM targets" OFF)

# RHI Presentation (Windowing) Layers
option(RRL_BUILD_WINDOW_OPENCV      "Build the OpenCV Window presentation layer" ON)
# option(RRL_BUILD_WINDOW_GLFW      "Build the GLFW Window presentation layer" OFF)



# --- Native ARCH -------------------------------------------------
if(RRL_ENABLE_NATIVE_ARCH)
    message(STATUS "RRL: Native architecture optimization enabled.")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=native)
    elseif(MSVC)
        # MSVC does not have a direct equivalent to -march=native. 
        # You typically have to specify /arch:AVX2 explicitly.
        # add_compile_options(/arch:AVX2) 
    endif()
endif()



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
    src/debug/RHIDebugger.cpp

)



# --- Base Dependencies -------------------------------------------
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

## Google Highway (SIMD Math)
if(RRL_BUILD_SWR_SIMD_BACKEND)
    find_package(hwy QUIET)
    if(hwy_FOUND OR HWY_FOUND OR TARGET hwy::hwy OR TARGET hwy)
        message(STATUS "RRL: Google Highway found. Enabling core SIMD capabilities.")
        list(APPEND RRL_PRIVATE_COMPILE_DEFS RRL_BUILD_SWR_SIMD_BACKEND)
        set(RRL_BUILD_SWR_SIMD_BACKEND TRUE)
        
        if(TARGET hwy::hwy)
            list(APPEND RRL_PRIVATE_LIBRARIES hwy::hwy)
        elseif(TARGET hwy)
            list(APPEND RRL_PRIVATE_LIBRARIES hwy)
        else()
            list(APPEND RRL_PRIVATE_LIBRARIES ${HWY_LIBRARIES})
        endif()
    else()
        message(STATUS "RRL: Google Highway not found. Defaulting to scalar fallback capabilities.")
        set(RRL_BUILD_SWR_SIMD_BACKEND OFF CACHE BOOL "Build the software rendering backend with SIMD acceleration" FORCE)
    endif()
endif()


# --- RHI Backends ------------------------------------------------
## Base Software Rendering + appropiate math
# Assign the proper math source based on the Highway dependency
list(APPEND RRL_SOURCES 
    src/rhi/software/SWRRasterizer.cpp
    src/rhi/software/SoftwareRenderer.cpp
)
if(RRL_BUILD_SWR_SIMD_BACKEND)
    list(APPEND RRL_SOURCES 
        src/rhi/software/SWRMath_Highway.cpp
        src/rhi/software/SWRRasterizer_Highway.cpp
    )
else()
    list(APPEND RRL_SOURCES 
        src/rhi/software/SWRMath_Scalar.cpp
        src/rhi/software/SWRRasterizer_Scalar.cpp
    )
endif()

## Target Dispatching
# WASM build
if(EMSCRIPTEN)
    set(RRL_WASM_TARGET TRUE)
    message(STATUS "RRL [Software RHI]: WebAssembly target detected.")
    message(WARNING "WASM NOT IMPLEMENTED YET")

# Desktop build
else()
    message(STATUS "RRL [Software RHI]: Desktop target detected.")
    set(RRL_DESKTOP_TARGET TRUE)
endif()


## Desktop Presentation: OpenCV Window
if(RRL_DESKTOP_TARGET AND RRL_BUILD_WINDOW_OPENCV)
    find_package(OpenCV QUIET)
    if(OpenCV_FOUND)
        message(STATUS "RRL [Presentation]: OpenCV found. Enabling OpenCV Window Support.")
        list(APPEND RRL_PRIVATE_LIBRARIES ${OpenCV_LIBS})
        list(APPEND RRL_PRIVATE_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
        list(APPEND RRL_PUBLIC_COMPILE_DEFS RRL_BUILD_WINDOW_OPENCV)
        list(APPEND RRL_SOURCES src/rhi/window/OpenCVWindow.cpp)
    else()
        message(WARNING "RRL [Presentation]: OpenCV not found! Disabling OpenCV Window support.")
        set(RRL_BUILD_WINDOW_OPENCV OFF CACHE BOOL "Build the OpenCV Window presentation layer" FORCE)
    endif()
endif()



## Desktop OpenGL Rendering
if(RRL_DESKTOP_TARGET AND RRL_BUILD_BACKEND_OPENGL)
    message(STATUS "RRL [Hardware RHI]: OpenGL backend enabled.")
    list(APPEND RRL_PUBLIC_COMPILE_DEFS RRL_BUILD_BACKEND_OPENGL)
    # find_package(OpenGL REQUIRED)
    # list(APPEND RRL_PRIVATE_LIBRARIES OpenGL::GL)
    # list(APPEND RRL_SOURCES src/rhi/opengl/OpenGLRenderer.cpp)
    # if(OpenGL_FOUND)
    # else()
    #     message(WARNING "RRL [OpenGL RHI]: OpenGL not found! Disabling OpenGL backend.")
    #     set(RRL_BUILD_BACKEND_OPENGL OFF CACHE BOOL "Build the OpenCV Rasterizer RHI Backend" FORCE)
    # endif()
endif()

## WASM WebGPU Rendering
if(RRL_WASM_TARGET AND RRL_BUILD_BACKEND_WEBGPU)
    message(STATUS "RRL [Hardware RHI]: WebGPU backend enabled for WebAssembly target.")
    list(APPEND RRL_PUBLIC_COMPILE_DEFS RRL_BUILD_BACKEND_WEBGPU)
    
    # Emscripten specific flags for WebGPU
    # list(APPEND RRL_PRIVATE_COMPILE_OPTIONS "-s USE_WEBGPU=1")
    # list(APPEND RRL_PRIVATE_LINK_OPTIONS "-s USE_WEBGPU=1")
    # list(APPEND RRL_SOURCES src/rhi/webgpu/WebGPURenderer.cpp)
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


# --- Subdirectories ----------------------------------------------
if(RRL_TESTING)
    message(STATUS "RRL: Configuring Tests...")
    add_subdirectory(test)
endif()

if(RRL_EXAMPLES)
    message(STATUS "RRL: Configuring Examples...")
    add_subdirectory(examples)
endif()

if(RRL_BENCHMARKS)
    message(STATUS "RRL: Configuring Benchmarks...")
    add_subdirectory(benchmarks)
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


