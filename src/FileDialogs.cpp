#include "header/FileDialogs.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

namespace FileDialogs {

    std::string OpenRomDialog(GLFWwindow* window)
    {
#ifdef _WIN32
        HWND owner = glfwGetWin32Window(window);

        char fileName[MAX_PATH] = { 0 };

        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));

        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = owner;
        ofn.lpstrFile    = fileName;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrFilter  = "NES ROM (*.nes)\0*.nes\0All Files (*.*)\0*.*\0\0";
        ofn.nFilterIndex = 1;
        ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt  = "nes";

        if (GetOpenFileNameA(&ofn) == TRUE)
            return std::string(fileName);

        return {};
#else
        (void)window;
        return {};
#endif
    }

} // namespace FileDialogs
