#pragma once
#include <string>

struct GLFWwindow;

namespace FileDialogs {
    // Returns empty string if cancelled
    std::string OpenRomDialog(GLFWwindow* window);
}