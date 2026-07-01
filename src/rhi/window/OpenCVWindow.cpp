#ifndef RRL_BUILD_WINDOW_OPENCV
#error "This file (OpenCVWindow.cpp) requires OpenCV support. Please build the library linking OpenCV."
#endif

#include "RRL/rhi/RHIBackend.hpp"
#include <opencv2/highgui.hpp>

namespace rrl::rhi::window::opencv {

// --- Lifecycle API -----------------------------------------------
bool Initialize(RHIWindow& window, const char* title, uint32_t w, uint32_t h) {
    window.width = w;
    window.height = h;

    // store the name so Present() can look it up
    window.native_handle = const_cast<char*>(title); 
    cv::namedWindow(title, cv::WINDOW_AUTOSIZE);
    return true;
}

bool PollEvents(RHIWindow& window) {
    int key = cv::waitKey(1);
    return key != 27; // false if ESC is pressed
}

void Shutdown(RHIWindow& window) {
    if (window.native_handle != nullptr) {
        const char* title = static_cast<const char*>(window.native_handle);
        cv::destroyWindow(title);
    }
}


} // namespace rrl::rhi::window::opencv