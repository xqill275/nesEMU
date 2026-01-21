#pragma once
#include <memory>
#include <string>

struct GLFWwindow;

#include "cpu.h"
#include "Bus.h"
#include "ppu.h"
#include "cartridge.h"
#include "Keybinds.h"
#include "GLTextures.h"

class EmuApp {
public:
    bool init();
    int  run();
    void shutdown();

private:
    bool loadRom(const std::string& path);

    void beginImGuiFrame();
    void endImGuiFrame();

    void drawMenuBar();
    void drawPanels();

    void tickEmulation();

private:
    GLFWwindow* window = nullptr;

    cpu CPU;
    ppu PPU;
    bus BUS;

    std::unique_ptr<cartridge> CART;
    std::string loadedRomPath;

    Keybinds binds;
    std::string bindsPath = "keybinds.cfg";

    GLTextures textures;

    // UI state
    bool running = false;
    bool openKeybindsPopup = false;

    bool showCPU = true;
    bool showMemory = false;
    bool showStack = false;
    bool showPPU = true;
    bool showVRAM = false;
    bool showPattern = true;

    // timing
    double lastTime = 0.0;
    double accumulator = 0.0;
    const double targetFrameTime = 1.0 / 60.0;
};
