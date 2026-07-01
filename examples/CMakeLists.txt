# examples/CMakeLists.txt
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


if (NOT RRL_EXAMPLES) 
    return()
endif()
if (NOT RRL_DESKTOP_TARGET)
    message(WARNING "RRL [Examples]: Examples are currently only supported natively on Desktop targets. Skipping.")
    return()
endif()



# --- Example External Dependencies -------------------------------
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)


# --- Example Dependencies ----------------------------------------
set(RRL_EXAMPLES_LIBS RRL::rrl Threads::Threads Boost::boost)
set(RRL_EXAMPLES_INCLUDES )


if(RRL_BUILD_WINDOW_OPENCV AND OpenCV_FOUND)
    list(APPEND RRL_EXAMPLES_LIBS ${OpenCV_LIBS})
    list(APPEND RRL_EXAMPLES_INCLUDES ${OpenCV_INCLUDE_DIRS})
endif()

if(RRL_BUILD_BACKEND_OPENGL)
    # list(APPEND RRL_EXAMPLES_LIBS OpenGL::GL glfw)
endif()



# --- Example Executable Targets ----------------------------------
# Collect all example C++ source files directly in this folder and subfolders
file(GLOB_RECURSE RRL_EXAMPLE_SOURCES "*.cpp" "*.cc")
if(RRL_EXAMPLE_SOURCES)

    foreach(example_file IN LISTS RRL_EXAMPLE_SOURCES)
        file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${example_file})
        message(STATUS "RRL [Examples]: Found example source file: ${rel_path}")
        get_filename_component(example_name ${example_file} NAME_WE)
        set(target_name "ex_${example_name}")
        
        add_executable(${target_name} ${example_file})
        
        # Link all aggregated libraries and includes
        target_link_libraries(${target_name} PRIVATE ${RRL_EXAMPLES_LIBS})
        if(RRL_EXAMPLES_INCLUDES)
            target_include_directories(${target_name} PRIVATE ${RRL_EXAMPLES_INCLUDES})
        endif()
        
    endforeach()

else()
    message(WARNING "RRL [Examples]: RRL_EXAMPLES is ON, but no example source files were found in the 'examples/' directory.")
endif()

