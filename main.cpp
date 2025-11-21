#include <iostream>

// GLAD
#include "src/external/glad/include/glad/glad.h"

#include <GLFW/glfw3.h>

#include "src/header/cpu.h"
#include "src/header/Bus.h"

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
        ImGui::Text("P: %02X", cpu.P);

        if (ImGui::Button("Step CPU")) {
            cpu.execute();
        }

        ImGui::End();

        // ------------------------
        // RAM View
        // ------------------------
        ImGui::Begin("RAM");

        for (int i = 0; i < 256; i++) {
            if (i % 16 == 0)
                ImGui::Text("\n%04X: ", i);
            ImGui::SameLine();
            ImGui::Text("%02X ", bus.ram[i]);
        }

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
