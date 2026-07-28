# ==============================================================================
# @Project:     Flexible Logging Library (FLogging)
# @Author:      https://github.com/alanglk
# @Date:        11/06/2026
# @Description: CMake configuration for the Flogging Lib
# @Version:     1.0.0
# ==============================================================================

cmake_minimum_required(VERSION 3.15)

project(FLogging
    VERSION 1.0.0 
    DESCRIPTION "Flexible Logging Library (FLogging)"
    HOMEPAGE_URL "https://github.com/alanglk/FLogging"
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Check if FLogging is the main project or bundled via add_subdirectory
set(FLOGGING_IS_MASTER_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(FLOGGING_IS_MASTER_PROJECT ON)
endif()

# --- Compilation out paths ---------------------------------------
if(FLOGGING_IS_MASTER_PROJECT)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
endif()


# --- Building options --------------------------------------------
option(FLOGGING_SHARED          "Build FLogging as a shared library" ON)
option(FLOGGING_F_PREFIX        "Require F_ prefix for logging macros to avoid collisions" OFF)
option(FLOGGING_QUILL_BACKEND   "Use Quill backend for maximum performance" OFF)
option(FLOGGING_TESTING         "Build tests" OFF)


# --- Installation Variables --------------------------------------
include(GNUInstallDirs)
if(FLOGGING_IS_MASTER_PROJECT)
    # Install tree inside the build dir by default <build>/dist
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist" CACHE PATH "Install path" FORCE)
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
set(FLOGGING_TYPE STATIC)                # STATIC or SHARED
set(FLOGGING_PUBLIC_LIBRARIES )          # Public FLOGGING dependencies (entt, glm)
set(FLOGGING_PRIVATE_LIBRARIES )         # Private FLOGGING dependencies (no deps for now)
set(FLOGGING_PUBLIC_INCLUDE_DIRS )       # Public FLOGGING headers
set(FLOGGING_PUBLIC_COMPILE_DEFS )       
set(FLOGGING_PRIVATE_COMPILE_DEFS )      # e.g. ENABLE_TESTING compile definition to compile tests source sections

if(FLOGGING_SHARED)
    set(FLOGGING_TYPE SHARED)
endif()
if(FLOGGING_F_PREFIX)
    list(APPEND FLOGGING_PUBLIC_COMPILE_DEFS FLOGGING_USE_F_PREFIX)
endif()


# --- Enable Testing (Fetch GTest) --------------------------------
if(FLOGGING_TESTING)
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

    list(APPEND FLOGGING_PRIVATE_COMPILE_DEFS ENABLE_TESTING)
endif()


# --- Library Dependencies ----------------------------------------
## FMT (formating C++17)
find_package(fmt REQUIRED)
list(APPEND FLOGGING_PUBLIC_LIBRARIES fmt::fmt)

## Boost Support (Boost.Asio in NetworkUDPSink)
find_package(Boost REQUIRED)
list(APPEND FLOGGING_PRIVATE_LIBRARIES Boost::headers)


## Quill: located in third_party/quill 
if (FLOGGING_QUILL_BACKEND)
    if(NOT TARGET quill::quill)
        set(QUILL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quill")
        if(EXISTS "${QUILL_DIR}/CMakeLists.txt")
            set(QUILL_INSTALL OFF CACHE BOOL "" FORCE)
            add_subdirectory("${QUILL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/quill" EXCLUDE_FROM_ALL)
            set_target_properties(quill PROPERTIES POSITION_INDEPENDENT_CODE ON)
            list(APPEND FLOGGING_PUBLIC_LIBRARIES quill::quill)
            list(APPEND FLOGGING_PUBLIC_COMPILE_DEFS ENABLE_QUILL)
            list(APPEND FLOGGING_PUBLIC_INCLUDE_DIRS "${QUILL_DIR}/include")
        else()
            message(WARNING "Quill source missing. Run: git submodule update --init --recursive")
        endif()
    endif()
    
endif()


# --- Library Sources ---------------------------------------------
set(FLOGGING_SOURCES
    src/FLogging.cpp
)


# --- Library Target ----------------------------------------------
add_library(flogging ${FLOGGING_TYPE} ${FLOGGING_SOURCES})
set_target_properties(flogging PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(GenerateExportHeader)
generate_export_header(flogging
    EXPORT_MACRO_NAME FLOGGING_API
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/FLogging/flogging_export.h"
)
target_include_directories(flogging 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)
if(FLOGGING_PUBLIC_INCLUDE_DIRS)
    target_include_directories(flogging PUBLIC $<BUILD_INTERFACE:${FLOGGING_PUBLIC_INCLUDE_DIRS}>)
endif()

target_compile_definitions(flogging 
    PUBLIC  ${FLOGGING_PUBLIC_COMPILE_DEFS}
    PRIVATE ${FLOGGING_PRIVATE_COMPILE_DEFS}
)
target_link_libraries(flogging 
    PUBLIC  ${FLOGGING_PUBLIC_LIBRARIES}
    PRIVATE ${FLOGGING_PRIVATE_LIBRARIES}
)
add_library(FLogging::flogging ALIAS flogging)


# --- Test Executable Targets -------------------------------------
if(FLOGGING_TESTING)
    # collect all test C++ source files in the test/ folder
    file(GLOB_RECURSE FLOGGING_TEST_SOURCES "test/*.cpp" "test/*.cc")
    if(FLOGGING_TEST_SOURCES)

        foreach(test_file IN LISTS FLOGGING_TEST_SOURCES)
            file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})
            message(STATUS "Found test source file: ${rel_path}")
        endforeach()

        add_executable(flogging_tests ${FLOGGING_TEST_SOURCES})
        target_link_libraries(flogging_tests 
            PRIVATE 
                flogging
                GTest::gtest_main
        )
        
        # Discover and add tests to ctest automatically
        include(GoogleTest)
        gtest_discover_tests(flogging_tests)
    else()
        message(WARNING "Testing is enabled, but no test source files were found in 'test/'.")
    endif()
endif()



# --- Install -----------------------------------------------------
include(GNUInstallDirs)

install(
  TARGETS flogging
  EXPORT FLoggingTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(FLOGGING_QUILL_BACKEND AND TARGET quill::quill)
    install(TARGETS quill EXPORT FLoggingTargets 
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(FLOGGING_QUILL_BACKEND AND TARGET quill::quill)
  install(DIRECTORY ${QUILL_DIR}/include/quill DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

install(
  EXPORT FLoggingTargets 
  FILE  FLoggingTargets.cmake
  NAMESPACE  FLogging::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FLogging
)


include(CMakePackageConfigHelpers)

# Version File 
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/FLoggingConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Config File
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/FLoggingConfig.cmake "
include(CMakeFindDependencyMacro)
find_dependency(Threads)
find_dependency(fmt)
include(\"\${CMAKE_CURRENT_LIST_DIR}/FLoggingTargets.cmake\")
")

# Install Both
install(
    FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/FLoggingConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/FLoggingConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/FLogging
)