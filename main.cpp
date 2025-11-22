#include <iostream>

// GLAD
#include "src/external/glad/include/glad/glad.h"

#include <GLFW/glfw3.h>

#include "src/header/cpu.h"
#include "src/header/Bus.h"
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
    bus bus;

    cpu.connectBus(&bus);
    bus.connectCpu(&cpu);


    cartridge cart("C:/Users/olive/CLionProjects/untitled1/roms/donkeyKong.nes");   // <-- your .nes file


    bus.insertCartridge(&cart);

    cpu.reset();
    bus.reset();

    // -----------------------------
    // Main Loop
    // -----------------------------
    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ------------------------
        //  CPU Window
        // ------------------------
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
            cpu.stepInstruction();
        }
        bool running = false;
        if (ImGui::Button("run CPU")) {
            running = !running;
        }
        if (running == true) {
            cpu.clock();
        }


        ImGui::End();

        // ------------------------
        // RAM View
        // ------------------------
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


        // ------------------------
        // Stack Viewer
        // ------------------------
        ImGui::Begin("Stack");

        cpu.drawStackGui();

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
