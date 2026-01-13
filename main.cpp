#include <iostream>

// GLAD
#include "src/external/glad/include/glad/glad.h"

#include <GLFW/glfw3.h>

#include "src/header/cpu.h"
#include "src/header/Bus.h"
#include "src/header/ppu.h"
#include "src/header/cartridge.h"

// ImGui includes
#include "src/external/imgui/imgui.h"
#include "src/external/imgui/backends/imgui_impl_glfw.h"
#include "src/external/imgui/backends/imgui_impl_opengl3.h"



int main() {
    // -----------------------------
    // Initialize GLFW
    // -----------------------------
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "NES Emulator GUI", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // -----------------------------
    // Load GLAD
    // -----------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    // -----------------------------
    // Initialize ImGui
    // -----------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // -----------------------------
    // Create CPU / BUS
    // -----------------------------
    cpu cpu;
    ppu ppu;
    bus bus;

    cpu.connectBus(&bus);
    bus.connectCpu(&cpu);
    bus.connectPPU(&ppu);


    // -----------------------------
    // Create PPU Framebuffer Texture (ONCE)
    // -----------------------------
    GLuint framebufferTex;
    glGenTextures(1, &framebufferTex);
    glBindTexture(GL_TEXTURE_2D, framebufferTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocate storage once
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        256,
        240,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    // -----------------------------
    // Create PPU Pattern Table Textures (ONCE)
    // -----------------------------
    GLuint patternTex[2];
    glGenTextures(2, patternTex);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, patternTex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    cartridge cart("C:/Users/olive/CLionProjects/untitled1/roms/mario.nes");



    bus.insertCartridge(&cart);

    bus.reset();
    ppu.PPUMASK = 0x18;
    bool running = false;

    bool showCPU = true;
    bool showMemory = false;
    bool showStack = false;
    bool showPPU = true;
    bool showVRAM = false;
    bool showPattern = true;

    // -----------------------------
    // Main Loop
    // -----------------------------
    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // -----------------------------
        // Menu Bar
        // -----------------------------

        if (ImGui::BeginMainMenuBar()) {
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

        ppu.renderBackground();
        ppu.renderSprites();

        glBindTexture(GL_TEXTURE_2D, framebufferTex);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            256,
            240,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            ppu.frame.data()
        );
        // ------------------------
        // Update PPU Pattern Tables
        // ------------------------
        if (showPattern) {
            ppu.updatePatternTable();

            for (int i = 0; i < 2; i++) {
                glBindTexture(GL_TEXTURE_2D, patternTex[i]);
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGBA,
                    128,
                    128,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    ppu.patternTable[i].data()
                );
            }
        }


        // ------------------------
        //  CPU Window
        // ------------------------
        if (showCPU) {
            ImGui::Begin("CPU Registers");

            ImGui::Text("A: %02X", cpu.A);
            ImGui::Text("X: %02X", cpu.X);
            ImGui::Text("Y: %02X", cpu.Y);
            ImGui::Text("SP: %02X", cpu.SP);
            ImGui::Text("PC: %04X", cpu.PC);
            ImGui::Text("P: %04X", cpu.P);
            ImGui::Separator();
            ImGui::Text("Status Flags:");
            cpu.drawFlagsGui();
            ImGui::Separator();

            if (ImGui::Button("Step CPU")) {

                // If we're already complete, clock until a new instruction starts
                do {
                    bus.clock();
                } while (cpu.complete());

                // Now clock until that instruction finishes
                do {
                    bus.clock();
                } while (!cpu.complete());
            }


            if (ImGui::Button(running ? "Pause" : "Run")) {
                running = !running;
            }

            if (running) {
                // Run roughly one frame worth of cycles
                for (int i = 0; i < 30000; i++) {
                    bus.clock();
                }
            }
            ImGui::End();
        }


            // ------------------------
            // RAM View
            // ------------------------
        if (showMemory) {
            ImGui::Begin("Memory (PC View)");

            uint16_t start = cpu.PC;   // starting address in CPU memory space

            for (int i = 0; i < 256; i++) {
                uint16_t addr = start + i;
                uint8_t value = bus.read(addr, true);   // true = readonly mode

                if (i % 16 == 0) {
                    ImGui::Text("\n%04X: ", addr);
                }

                ImGui::SameLine();
                ImGui::Text("%02X ", value);
            }

            ImGui::End();
        }

        // ------------------------
        // PPU VRAM Viewer
        // ------------------------
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
                    uint8_t value = ppu.vram[row + col];
                    ImGui::SameLine();
                    ImGui::Text("%02X", value);
                }
            }

            ImGui::EndChild();
            ImGui::End();
        }

        if (showStack) {
            // ------------------------
            // Stack Viewer
            // ------------------------
            ImGui::Begin("Stack");

            cpu.drawStackGui();

            ImGui::End();
        }
        if (showPPU) {

            // ------------------------
            // PPU Viewer
            // ------------------------
            ImGui::Begin("PPU");

            ImGui::Text("Registers");
            ImGui::Separator();

            ImGui::Text("PPUCTRL   ($2000): %02X", ppu.PPUCTRL);
            ImGui::Text("PPUMASK   ($2001): %02X", ppu.PPUMASK);
            ImGui::Text("PPUSTATUS ($2002): %02X", ppu.PPUSTATUS);
            ImGui::Text("OAMADDR ($2003): %02X", ppu.OAMADDR);



            ImGui::Separator();
            ImGui::Text("Decoded PPUCTRL");

            ImGui::BulletText("NMI Enable: %s", (ppu.PPUCTRL & 0x80) ? "ON" : "OFF");
            ImGui::BulletText("Sprite Pattern Table: %s",
                (ppu.PPUCTRL & 0x08) ? "$1000" : "$0000");
            ImGui::BulletText("Background Pattern Table: %s",
                (ppu.PPUCTRL & 0x10) ? "$1000" : "$0000");
            ImGui::BulletText("Increment Mode: %s",
                (ppu.PPUCTRL & 0x04) ? "32" : "1");

            ImGui::Separator();
            ImGui::Text("Internal State");

            ImGui::Text("VRAM Addr: %04X", ppu.vram_addr);
            ImGui::Text("TRAM Addr: %04X", ppu.tram_addr);
            ImGui::Text("Addr Latch: %d", ppu.addr_latch);

            ImGui::Separator();
            ImGui::Text("Timing");

            ImGui::Text("Scanline: %d", ppu.scanline);
            ImGui::Text("Cycle: %d", ppu.cycle);
            ImGui::Text("NMI Line: %s", ppu.nmi ? "ASSERTED" : "clear");

            ImGui::End();
        }

        // ------------------------
        // Pattern Table Viewer
        // ------------------------
        if (showPattern) {
            ImGui::Begin("Pattern Tables");

            ImGui::Text("Pattern Table 0 ($0000)");
            ImGui::Image(
                (void*)(intptr_t)patternTex[0],
                ImVec2(256, 256)
            );

            ImGui::Separator();

            ImGui::Text("Pattern Table 1 ($1000)");
            ImGui::Image(
                (void*)(intptr_t)patternTex[1],
                ImVec2(256, 256)
            );

            ImGui::End();
        }

        ImGui::Begin("NES Screen");

        ImGui::Image(
            (void*)(intptr_t)framebufferTex,
            ImVec2(512, 480)   // scale 2× for readability
        );

        ImGui::End();

            // ------------------------
            // Rendering
            // ------------------------
            ImGui::Render();
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }

        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();

        return 0;
    }
