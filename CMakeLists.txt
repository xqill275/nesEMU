cmake_minimum_required(VERSION 3.20)
project(nesEMU)

set(CMAKE_CXX_STANDARD 17)

# -------------------------------------
# GLFW
# -------------------------------------
add_subdirectory(src/external/glfw)

# -------------------------------------
# GLAD
# -------------------------------------
add_library(glad STATIC
        src/external/glad/src/glad.c
)
target_include_directories(glad PUBLIC
        src/external/glad/include
)

# -------------------------------------
# ImGui
# -------------------------------------
set(IMGUI_SRC
        src/external/imgui/imgui.cpp
        src/external/imgui/imgui_draw.cpp
        src/external/imgui/imgui_widgets.cpp
        src/external/imgui/imgui_tables.cpp
        src/external/imgui/imgui_demo.cpp

        src/external/imgui/backends/imgui_impl_glfw.cpp
        src/external/imgui/backends/imgui_impl_opengl3.cpp
        src/Mappers/Mapper000.h
        src/header/ppu.h
        src/header/FileDialogs.h
        src/header/GLTextures.h
        src/AudioOut.cpp
        src/header/AudioOut.h
)

add_library(imgui STATIC ${IMGUI_SRC})

target_include_directories(imgui PUBLIC
        src/external/imgui
        src/external/imgui/backends
        src/external/glfw/include
        src/external/glad/include
)

# -------------------------------------
# Emulator Source Files
# -------------------------------------
set(SOURCES
        src/Bus.cpp
        src/cpu.cpp
        main.cpp
        src/cartridge.cpp
        src/header/cartridge.h
        src/Mappers/Mapper000.h
        src/mapper.cpp
        src/header/mapper.h
        src/Mappers/Mapper000.cpp
        src/ppu.cpp
        src/header/KeyBinds.h
        src/header/FileDialogs.h
        src/FileDialogs.cpp
        src/header/GLTextures.h
        src/GLTextures.cpp
        src/header/KeybindsUI.h
        src/KeybindsUI.cpp
        src/header/EmuApp.h
        src/EmuApp.cpp
        src/header/apu.h
        src/apu.cpp
        src/external/miniaudio/miniaudio.h
        src/AudioOut.cpp
        src/header/AudioOut.h
)

# Create executable (IMPORTANT!)
add_executable(nesEMU ${SOURCES})

target_include_directories(nesEMU PUBLIC
        src/header
        src/external/glad/include
        src/external/glfw/include
        src/external/imgui
)

# -------------------------------------
# Link everything
# -------------------------------------
target_link_libraries(nesEMU
        PRIVATE
        glfw
        glad
        imgui
)

# Add OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(nesEMU PRIVATE OpenGL::GL)
