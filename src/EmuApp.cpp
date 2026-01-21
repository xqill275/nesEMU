#include "header/EmuApp.h"

#include <iostream>

// GLAD
#include "external/glad/include/glad/glad.h"
#include <GLFW/glfw3.h>

// ImGui
#include "external/imgui/imgui.h"
#include "external/imgui/backends/imgui_impl_glfw.h"
#include "external/imgui/backends/imgui_impl_opengl3.h"

#include "header/FileDialogs.h"
#include "header/KeybindsUI.h"

#ifdef _WIN32
#include <windows.h>
#endif

bool EmuApp::init()
{
    // GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    window = glfwCreateWindow(1280, 720, "NES Emulator GUI", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Wire emulator
    CPU.connectBus(&BUS);
    BUS.connectCpu(&CPU);
    BUS.connectPPU(&PPU);

    // Keybinds
    binds = Keybinds::Defaults();
    if (!LoadKeybinds(binds, bindsPath)) {
        SaveKeybinds(binds, bindsPath);
    }
    KeybindsUI::Init(bindsPath);

    // Textures
    textures.init();

    // timing
    lastTime = glfwGetTime();
    accumulator = 0.0;

    return true;
}

void EmuApp::shutdown()
{
    textures.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

bool EmuApp::loadRom(const std::string& path)
{
    if (path.empty()) return false;

    auto newCart = std::make_unique<cartridge>(path);
    if (!newCart->valid) return false;

    CART = std::move(newCart);
    BUS.insertCartridge(CART.get());

    BUS.reset();
    PPU.frame_complete = false;
    loadedRomPath = path;

    return true;
}

void EmuApp::beginImGuiFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EmuApp::endImGuiFrame()
{
    ImGui::Render();
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void EmuApp::tickEmulation()
{
    // controller every frame
    BUS.setControllerState(0, BuildControllerByte(binds));

    // shortcuts
    if (ImGui::IsKeyPressed(binds.runGame)) running = !running;
    if (ImGui::IsKeyPressed(binds.resetGame)) BUS.reset();

    if (!running) return;

    double now = glfwGetTime();
    double delta = now - lastTime;
    lastTime = now;

    if (delta > 0.25) delta = 0.25;

    accumulator += delta;

    while (accumulator >= targetFrameTime) {
        PPU.frame_complete = false;
        while (!PPU.frame_complete) {
            BUS.clock();
        }
        accumulator -= targetFrameTime;
    }
}

void EmuApp::drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open ROM...")) {
            std::string path = FileDialogs::OpenRomDialog(window);
            if (!path.empty()) {
                if (!loadRom(path)) {
#ifdef _WIN32
                    MessageBoxA(nullptr, "Failed to load ROM.", "Error", MB_OK | MB_ICONERROR);
#endif
                } else {
                    running = false; // optional pause after load
                }
            }
        }

        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Game")) {
        if (ImGui::MenuItem(running ? "Pause" : "Run", ImGui::GetKeyName(binds.runGame))) running = !running;
        if (ImGui::MenuItem("Reset Game", ImGui::GetKeyName(binds.resetGame))) BUS.reset();

        if (ImGui::MenuItem("Step Instruction")) {
            do { BUS.clock(); } while (CPU.complete());
            do { BUS.clock(); } while (!CPU.complete());
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Settings")) {
        if (ImGui::MenuItem("Change Keybinds...")) openKeybindsPopup = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("CPU", nullptr, &showCPU);
        ImGui::MenuItem("Memory", nullptr, &showMemory);
        ImGui::MenuItem("Stack", nullptr, &showStack);
        ImGui::MenuItem("PPU", nullptr, &showPPU);
        ImGui::MenuItem("VRAM", nullptr, &showVRAM);
        ImGui::MenuItem("Pattern Tables", nullptr, &showPattern);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EmuApp::drawPanels()
{
    // Keybinds popup
    KeybindsUI::DrawPopup(binds, openKeybindsPopup);

    // Draw latest frame
    PPU.renderBackground();
    PPU.renderSprites();
    textures.uploadFrameBGRA(PPU.frame.data());

    // Pattern tables
    if (showPattern) {
        PPU.updatePatternTable();
        textures.uploadPatternBGRA(0, PPU.patternTable[0].data());
        textures.uploadPatternBGRA(1, PPU.patternTable[1].data());
    }

    // CPU
    if (showCPU) {
        ImGui::Begin("CPU Registers");
        ImGui::Text("A: %02X", CPU.A);
        ImGui::Text("X: %02X", CPU.X);
        ImGui::Text("Y: %02X", CPU.Y);
        ImGui::Text("SP: %02X", CPU.SP);
        ImGui::Text("PC: %04X", CPU.PC);
        ImGui::Text("P: %02X", CPU.P);
        ImGui::Separator();
        ImGui::Text("Status Flags:");
        CPU.drawFlagsGui();
        ImGui::End();
    }

    // Memory
    if (showMemory) {
        ImGui::Begin("Memory (PC View)");
        uint16_t start = CPU.PC;
        for (int i = 0; i < 256; i++) {
            uint16_t addr = start + i;
            uint8_t value = BUS.read(addr, true);
            if (i % 16 == 0) ImGui::Text("\n%04X: ", addr);
            ImGui::SameLine();
            ImGui::Text("%02X ", value);
        }
        ImGui::End();
    }

    // Stack
    if (showStack) {
        ImGui::Begin("Stack");
        CPU.drawStackGui();
        ImGui::End();
    }

    // VRAM
    if (showVRAM) {
        ImGui::Begin("PPU VRAM ($2000-$27FF)");
        ImGui::Text("Nametable VRAM (2 KB)");
        ImGui::Separator();

        ImGui::BeginChild("VRAMScroll", ImVec2(0, 400), true);
        for (int row = 0; row < 2048; row += 16) {
            uint16_t addr = 0x2000 + row;
            ImGui::Text("%04X:", addr);
            ImGui::SameLine();
            for (int col = 0; col < 16; col++) {
                uint8_t value = PPU.vram[row + col];
                ImGui::SameLine();
                ImGui::Text("%02X", value);
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    // PPU
    if (showPPU) {
        ImGui::Begin("PPU");

        ImGui::Text("Registers");
        ImGui::Separator();

        ImGui::Text("PPUCTRL   ($2000): %02X", PPU.PPUCTRL);
        ImGui::Text("PPUMASK   ($2001): %02X", PPU.PPUMASK);
        ImGui::Text("PPUSTATUS ($2002): %02X", PPU.PPUSTATUS);
        ImGui::Text("OAMADDR   ($2003): %02X", PPU.OAMADDR);

        ImGui::Separator();
        ImGui::Text("Decoded PPUCTRL");
        ImGui::BulletText("NMI Enable: %s", (PPU.PPUCTRL & 0x80) ? "ON" : "OFF");
        ImGui::BulletText("Sprite Pattern Table: %s", (PPU.PPUCTRL & 0x08) ? "$1000" : "$0000");
        ImGui::BulletText("Background Pattern Table: %s", (PPU.PPUCTRL & 0x10) ? "$1000" : "$0000");
        ImGui::BulletText("Increment Mode: %s", (PPU.PPUCTRL & 0x04) ? "32" : "1");

        ImGui::Separator();
        ImGui::Text("Internal State");
        ImGui::Text("VRAM Addr: %04X", PPU.vram_addr.reg);
        ImGui::Text("TRAM Addr: %04X", PPU.tram_addr.reg);
        ImGui::Text("Addr Latch: %d", PPU.addr_latch);

        ImGui::Separator();
        ImGui::Text("Timing");
        ImGui::Text("Scanline: %d", PPU.scanline);
        ImGui::Text("Cycle: %d", PPU.cycle);
        ImGui::Text("NMI Line: %s", PPU.nmi ? "ASSERTED" : "clear");

        ImGui::End();
    }

    // Pattern viewer
    if (showPattern) {
        ImGui::Begin("Pattern Tables");
        ImGui::Text("Pattern Table 0 ($0000)");
        ImGui::Image((void*)(intptr_t)textures.patternTex(0), ImVec2(256, 256));
        ImGui::Separator();
        ImGui::Text("Pattern Table 1 ($1000)");
        ImGui::Image((void*)(intptr_t)textures.patternTex(1), ImVec2(256, 256));
        ImGui::End();
    }

    // NES screen
    ImGui::Begin("NES Screen");
    ImGui::Image((void*)(intptr_t)textures.frameTex(), ImVec2(512, 480));
    ImGui::End();
}

int EmuApp::run()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        beginImGuiFrame();

        drawMenuBar();

        // emu tick must happen after ImGui::NewFrame so IsKeyPressed works
        tickEmulation();

        drawPanels();

        endImGuiFrame();
    }

    return 0;
}
