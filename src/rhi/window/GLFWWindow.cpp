// src/rhi/window/GLFWWindow.cpp
#ifndef RRL_BUILD_WINDOW_GLFW
#error "This file (GLFWWindow.cpp) requires GLFW + OpenGL support. Please build the library linking GLFW2.4 and OpenGL4.5."
#endif
#ifndef RRL_BUILD_BACKEND_OPENGL
#error "This file (GLFWWindow.cpp) requires GLFW + OpenGL support. Please build the library linking GLFW2.4 and OpenGL4.5."
#endif

#include "RRL/rhi/RHIBackend.hpp"
#include "RRL/DebugMacros.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>


namespace rrl::rhi::window::glfw {

// --- Lifecycle API -----------------------------------------------
bool Initialize(RHIWindow& window, const char* title, uint32_t w, uint32_t h) {
    if (!glfwInit()) {
        RRL_ASSERT(false, "[GLFWWindow] Failed to initialize GLFW.");
        return false;
    }

    // Explicit window creation asks for a visible context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    GLFWwindow* gl_window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!gl_window) {
        RRL_ASSERT(false, "[GLFWWindow] Failed to create GLFW window.");
        return false;
    }

    window.width = w;
    window.height = h;
    window.native_handle = gl_window;
    return true;
}

bool PollEvents(RHIWindow& window) {
    if (window.native_handle) {
        glfwPollEvents();
        GLFWwindow* gl_window = static_cast<GLFWwindow*>(window.native_handle);
        return !glfwWindowShouldClose(gl_window); // Returns false when the user clicks 'X'
    }
    return false;
}

void Shutdown(RHIWindow& window) {
    if (window.native_handle != nullptr) {
        GLFWwindow* gl_window = static_cast<GLFWwindow*>(window.native_handle);
        glfwDestroyWindow(gl_window);
        window.native_handle = nullptr;

        // Do NOT call glfwTerminate() here, because the OpenGL backend 
        // might still be using a hidden context in the background!
    }
}


} // namespace rrl::rhi::window::glfw