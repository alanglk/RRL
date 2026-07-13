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
    LANGUAGES C CXX
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
option(RRL_DOCUMENTATION            "Build RRL doxygen documentation" OFF )
option(RRL_EXAMPLES                 "Build RRL examples" OFF)
option(RRL_BENCHMARKS               "Build RRL performance benchmarks" OFF)
option(RRL_CODE_COVERAGE            "Enable GCC/Clang code coverage tracking" OFF)
option(RRL_ENABLE_NATIVE_ARCH       "Optimize for the host CPU architecture (breaks portability)" OFF)
option(RRL_BUILD_SWR_SIMD_BACKEND   "Build the software rendering backend with SIMD acceleration" ON)

# RHI Backend Toggles
option(RRL_BUILD_BACKEND_OPENGL     "Build the hardware-accelerated OpenGL backend" OFF)
option(RRL_BUILD_BACKEND_WEBGPU     "Build the WebGPU backend for WASM targets" OFF)

# RHI Presentation (Windowing) Layers
option(RRL_BUILD_WINDOW_OPENCV      "Build the OpenCV Window presentation layer" OFF)
# Building OpenGL automatically enables GLFW windowing. 
# Building GLFW windowing without OpenGL backend is not supported.
# option(RRL_BUILD_WINDOW_GLFW      "Build the GLFW Window presentation layer" OFF) 

option(RRL_OPENGL_EMBED_SHADERS "Bake OpenGL shaders into the binary instead of loading from disk" OFF)


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
include(GNUInstallDirs)
if(RRL_IS_MASTER_PROJECT)
    # Install tree inside the build dir by default <build>/dist
    # ONLY if we are not in a release build
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        if(NOT CMAKE_BUILD_TYPE MATCHES "Release|MinSizeRel|RelWithDebInfo")
            set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist" CACHE PATH "Install path" FORCE)
        endif()
    endif()
endif()

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
set(CMAKE_SKIP_RPATH FALSE)

# OS-Specific Dynamic Linking Paths (RPATH)
if(APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@executable_path" "@executable_path/../lib")
else()
    set(CMAKE_MACOSX_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN" "$ORIGIN/../lib")
endif()

# Force MSVC to use the modern preprocessor
#   (variadic macros support)
if(MSVC)
    add_compile_options(/Zc:preprocessor)
endif()


# --- Library Setup Variables -------------------------------------
set(RRL_TYPE STATIC)                # STATIC or SHARED
set(RRL_SOURCES )                   # RRL sources
set(RRL_PUBLIC_LIBRARIES )          # Public RRL dependencies (entt, glm)
set(RRL_PRIVATE_LIBRARIES )         # Private RRL dependencies (glfw, ...)
set(RRL_PRIVATE_INCLUDE_DIRS )      # Private RRL include directories (tinyobjloader, stb header libs, glad...)
set(RRL_PUBLIC_COMPILE_DEFS )       # e.g. ENABLE_CUDA to compile CUDA RHI and tell lib linkers it is enabled
set(RRL_PRIVATE_COMPILE_DEFS )      # e.g. ENABLE_TESTING compile definition to compile tests source sections

if(RRL_SHARED)
    set(RRL_TYPE SHARED)
endif()


# --- Git Versioning ----------------------------------------------
set(RRL_GIT_HASH "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE RRL_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
message(STATUS "RRL: Configuring build for version ${PROJECT_VERSION} (Git: ${RRL_GIT_HASH})")
list(APPEND RRL_PUBLIC_COMPILE_DEFS
    RRL_VERSION="${PROJECT_VERSION}"
    RRL_GIT_HASH="${RRL_GIT_HASH}"
    FLOG_APP_VERSION="${PROJECT_VERSION}"
    FLOG_APP_GIT_HASH="${RRL_GIT_HASH}"
)


# --- Library Sources ---------------------------------------------
set(RRL_SOURCES

    # Asset functions implementations
    src/asset/AssetManager.cpp
    src/asset/SynchronizationSystems.cpp

    # Camera functions implementations
    src/camera/CameraSystem.cpp

    # RHI API main implementation
    src/rhi/RHI.cpp

    # Scene functions implementations
    src/scene/PrefabManager.cpp

    # TF functions implementations
    src/tf/TransformTree.cpp
    src/tf/TFComponents.cpp

    # IO functions implementations
    src/io/PrefabIO.cpp
    src/io/ImageIO.cpp


    # Debugging functions
    src/debug/AssetDebugger.cpp
    src/debug/RHIDebugger.cpp
    src/debug/SceneDebugger.cpp
    src/debug/TFDebugger.cpp


    # Publi API Facades
    src/modules/AssetModule.cpp
    src/modules/CameraModule.cpp
    src/modules/DebugModule.cpp
    src/modules/RHIModule.cpp
    src/modules/SceneModule.cpp
    src/modules/TFModule.cpp

    # RRL Engine object
    src/core/RRLEngine.cpp
)

# --- Library Private Base Includes -------------------------------
set(RRL_PRIVATE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include                 # Private RRL headers
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb             # STB header libs
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyobjloader   # OBJ file loader header lib
)



# --- Base Dependencies -------------------------------------------
## EnTT: located in third_party/entt 
set(ENTT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/entt")
if(EXISTS "${ENTT_DIR}/CMakeLists.txt")
    set(ENTT_INSTALL OFF CACHE BOOL "" FORCE) # Prevents EnTT overrides on install variables
    add_subdirectory(${ENTT_DIR} "${CMAKE_CURRENT_BINARY_DIR}/entt" EXCLUDE_FROM_ALL)
    list(APPEND RRL_PRIVATE_LIBRARIES $<BUILD_INTERFACE:EnTT::EnTT>)
else()
    message(FATAL_ERROR "EnTT source missing at ${ENTT_DIR}. Have you called 'git submodule update --init --recursive'?")
endif()

## GLM: located in third_party/glm 
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")
if(EXISTS "${GLM_DIR}/CMakeLists.txt")
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GLM_DIR} "${CMAKE_CURRENT_BINARY_DIR}/glm" EXCLUDE_FROM_ALL)
    list(APPEND RRL_PUBLIC_LIBRARIES $<BUILD_INTERFACE:glm::glm-header-only>)
    list(APPEND RRL_PUBLIC_COMPILE_DEFS GLM_ENABLE_EXPERIMENTAL)
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
    add_subdirectory(${FL_DIR} "${CMAKE_CURRENT_BINARY_DIR}/FLogging")
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
    message(STATUS "RRL [Hardware RHI]: OpenGL backend enabled (GLFW + GLAD 4.5 Core).")
    
    # OpenGL requires GLFW for generating its context
    set(RRL_BUILD_WINDOW_GLFW ON CACHE BOOL "Build the GLFW Window presentation layer" FORCE)
    
    # Include and Configure GLFW
    set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw")
    if(EXISTS "${GLFW_DIR}/CMakeLists.txt")
        # Disable GLFW clutter
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        
        add_subdirectory(${GLFW_DIR} "${CMAKE_CURRENT_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
        list(APPEND RRL_PRIVATE_LIBRARIES $<BUILD_INTERFACE:glfw>)
    else()
        message(FATAL_ERROR "GLFW source missing. Run 'git submodule update --init --recursive'")
        set(RRL_BUILD_WINDOW_GLFW OFF CACHE BOOL "Build the GLFW Window presentation layer" FORCE)
    endif()

    # Include GLAD
    set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
    if(EXISTS "${GLAD_DIR}/src/glad.c")
        list(APPEND RRL_SOURCES ${GLAD_DIR}/src/glad.c)
        list(APPEND RRL_PRIVATE_INCLUDE_DIRS ${GLAD_DIR}/include)
    else()
        message(FATAL_ERROR "GLAD source missing. Generate from glad.dav1d.de and place in third_party/glad/")
        set(RRL_BUILD_BACKEND_OPENGL OFF CACHE BOOL "Build the hardware-accelerated OpenGL backend" FORCE)
    endif()

endif()
if (RRL_BUILD_WINDOW_GLFW AND RRL_BUILD_BACKEND_OPENGL)
    list(APPEND RRL_SOURCES 
        src/rhi/opengl/Shader.cpp
        src/rhi/opengl/ShaderManager.cpp
        src/rhi/opengl/OpenGLRenderer.cpp
        src/rhi/window/GLFWWindow.cpp
    )
    list(APPEND RRL_PUBLIC_COMPILE_DEFS 
        RRL_BUILD_BACKEND_OPENGL
        RRL_BUILD_WINDOW_GLFW
    )
    
    # Shader configuration
    # If we are building at Release mode 
    #       -> shaders will be installed at:
    #           <system_install_dir>/RRL/shaders
    # If we are building on other mode:
    #       -> shaders will be installed at:
    #           <this_install_dir>/RRL/shaders
    #
    if (RRL_OPENGL_EMBED_SHADERS)
        message(STATUS "RRL [Hardware RHI]: Compiling with embedded shaders.")
        list(APPEND RRL_PRIVATE_COMPILE_DEFS RRL_OPENGL_EMBED_SHADERS)
    else()
        message(STATUS "RRL [Hardware RHI]: Compiling with external runtime shaders.")
        if(CMAKE_BUILD_TYPE MATCHES "Release|MinSizeRel|RelWithDebInfo")
            set(SHADER_BAKE_PATH "${CMAKE_INSTALL_FULL_DATADIR}/RRL/shaders")
        else()
            set(SHADER_BAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/opengl/shaders")
        endif()
        list(APPEND RRL_PRIVATE_COMPILE_DEFS 
            RRL_OPENGL_RUNTIME_SHADER_DIR="${SHADER_BAKE_PATH}"
        )
    endif()
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
        ${RRL_PRIVATE_INCLUDE_DIRS}
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
    enable_testing()
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
# Install Core Targets
install(
  TARGETS rrl
  EXPORT RRLTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install GLFW (if STATIC and enabled)
if(NOT RRL_SHARED)
    if(RRL_BUILD_WINDOW_GLFW)
        install(
            TARGETS glfw
            EXPORT RRLTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
endif()

# Install Public Headers (RRL)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install Public Dependency Headers (GLM)
install(DIRECTORY "${GLM_DIR}/glm" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl")

# Install Shaders (if not embedded)
if(RRL_BUILD_BACKEND_OPENGL AND NOT RRL_OPENGL_EMBED_SHADERS)
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/opengl/shaders/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/RRL/shaders
    )
endif()


# Export CMake targets
install(
  EXPORT RRLTargets 
  FILE  RRLTargets.cmake
  NAMESPACE RRL::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RRL
)

# Generate Config and Version files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/RRLConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Add a package CMake configuration file. 
#   This is useful for user-calls like: find_package(RRL REQUIRED)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RRLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/RRLConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RRL
)

install(
    FILES 
        "${CMAKE_CURRENT_BINARY_DIR}/RRLConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/RRLConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RRL
)



# --- Packaging (CPack) -------------------------------------------
if(RRL_IS_MASTER_PROJECT)
    set(CPACK_PACKAGE_NAME "RRL")
    set(CPACK_PACKAGE_VENDOR "alanglk")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Robotics Rendering Library (RRL)")
    
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    
    # Exclude files (safe guard, this should not be necessary)
    set(CPACK_SOURCE_IGNORE_FILES ".git/;/build/;/third_party/;.DS_Store/;/benchmarks/;/test/;/examples/")

    # OS Specific Generator Configuration
    if(WIN32)
        # Windows: Generates a .zip and a .msi installer
        set(CPACK_GENERATOR "ZIP;NSIS")
        set(CPACK_NSIS_MODIFY_PATH ON) 

    elseif(APPLE)
        # macOS: Generates a .tar.gz and a .dmg installer
        set(CPACK_GENERATOR "TGZ;DragNDrop")
        
    elseif(UNIX)
        # Linux: Generates a .tar.gz, a Debian (.deb), and a Red Hat (.rpm) installer
        set(CPACK_GENERATOR "TGZ;DEB;RPM")
        
        # Debian dependencies
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "alanglk")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libglfw3-dev")
        
        # Red Hat dependencies
        set(CPACK_RPM_PACKAGE_REQUIRES "glfw-devel")
    endif()

    include(CPack)
endif()


# --- Doxygen Documentation ---------------------------------------
if(RRL_DOCUMENTATION)
    if(CMAKE_BUILD_TYPE MATCHES "Release|MinSizeRel|RelWithDebInfo")
        find_package(Doxygen QUIET)
        if(DOXYGEN_FOUND)
            # Set input and output files
            set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
            set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

            # Request to configure the file
            configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
            message("Doxygen build started")

            # Note the option ALL which allows to build the docs together with the application
            add_custom_target( doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating RRL documentation with Doxygen"
                VERBATIM )
        else()
            set(RRL_DOCUMENTATION false)
            message(WARNING "RRL_DOCUMENTATION building option was set but Doxygen wasnt found on path. Skipping documentation build")
        endif()
    else()
        set(RRL_DOCUMENTATION false)
        message(WARNING "RRL_DOCUMENTATION building option was set but this is not a Release build. Skipping documentation build")
    endif()
endif()


# --- Code Coverage -----------------------------------------------
if(RRL_TESTING AND RRL_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Disable optimizations (-O0) so coverage lines match exactly with the source code
    message(STATUS "RRL: Code Coverage instrumentation enabled.")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()