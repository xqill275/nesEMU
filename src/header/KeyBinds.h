//
// Created by olive on 17/01/2026.
//

#ifndef KEYBINDS_H
#define KEYBINDS_H

#pragma once
#include <imgui.h>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdint>


struct Keybinds {
    // NES buttons
    ImGuiKey up;
    ImGuiKey down;
    ImGuiKey left;
    ImGuiKey right;
    ImGuiKey A;
    ImGuiKey B;
    ImGuiKey start;
    ImGuiKey select;

    // Emulator shortcuts
    ImGuiKey runGame;
    ImGuiKey resetGame;
    ImGuiKey stepGame;

    static Keybinds Defaults() {
        Keybinds k{};
        k.up     = ImGuiKey_W;
        k.down   = ImGuiKey_S;
        k.left   = ImGuiKey_A;
        k.right  = ImGuiKey_D;
        k.A      = ImGuiKey_E;
        k.B      = ImGuiKey_Q;
        k.start  = ImGuiKey_Enter;
        k.select = ImGuiKey_RightShift;

        k.runGame   = ImGuiKey_F5;
        k.resetGame = ImGuiKey_F1;
        k.stepGame = ImGuiKey_F6;
        return k;
    }
};

inline bool SaveKeybinds(const Keybinds& k, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "up=" << (int)k.up << "\n";
    out << "down=" << (int)k.down << "\n";
    out << "left=" << (int)k.left << "\n";
    out << "right=" << (int)k.right << "\n";
    out << "A=" << (int)k.A << "\n";
    out << "B=" << (int)k.B << "\n";
    out << "start=" << (int)k.start << "\n";
    out << "select=" << (int)k.select << "\n";
    out << "runGame=" << (int)k.runGame << "\n";
    out << "resetGame=" << (int)k.resetGame << "\n";
    out << "stepGame=" << (int)k.stepGame << "\n";
    return true;
}

inline bool LoadKeybinds(Keybinds& k, const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    // Start from defaults so missing lines still work
    k = Keybinds::Defaults();

    std::unordered_map<std::string, ImGuiKey*> map = {
        {"up", &k.up},
        {"down", &k.down},
        {"left", &k.left},
        {"right", &k.right},
        {"A", &k.A},
        {"B", &k.B},
        {"start", &k.start},
        {"select", &k.select},
        {"runGame", &k.runGame},
        {"resetGame", &k.resetGame},
        {"stepGame", &k.stepGame}
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        auto it = map.find(key);
        if (it == map.end()) continue;

        int code = 0;
        try { code = std::stoi(val); }
        catch (...) { continue; }

        *it->second = (ImGuiKey)code;
    }

    return true;
}

inline uint8_t BuildControllerByte(const Keybinds& k) {
    uint8_t state = 0x00;

    auto down = [](ImGuiKey key) { return ImGui::IsKeyDown(key); };

    if (down(k.A))      state |= (1 << 0);
    if (down(k.B))      state |= (1 << 1);
    if (down(k.select)) state |= (1 << 2);
    if (down(k.start))  state |= (1 << 3);
    if (down(k.up))     state |= (1 << 4);
    if (down(k.down))   state |= (1 << 5);
    if (down(k.left))   state |= (1 << 6);
    if (down(k.right))  state |= (1 << 7);

    return state;
}

#endif //KEYBINDS_H
