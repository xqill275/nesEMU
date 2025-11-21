//
// Created by olive on 21/11/2025.
//

#include "header/Bus.h"
#include "header/cpu.h"

bus::bus() {
    reset();
}

bus::~bus() = default;

void bus::connectCpu(cpu *cpu) {
    this->connectedCPU = cpu;
}


uint8_t bus::read(uint16_t addr, bool readonly) {
    uint8_t data = 0;
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        data = ram[addr & 0x07FF];
    } else {
        // TODO:
        // PPU: 0x2000–3FFF
        // APU + I/O: 0x4000–401F
        // Cartridge: >= 0x4020
        // For now, return 0
        data = 0x00;
    }
    return data;
}

void bus::write(uint16_t addr, uint8_t data) {
    if (addr >= 0x0000 && addr  <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
    } // TODO: other components memory bounds
}

void bus::reset() {
    for (auto& b : ram) {
        b = 0;
    }

    ram[0x0000] = 0xA9;  // LDA #$10
    ram[0x0001] = 0x10;

    ram[0x0002] = 0xA9;  // LDA #$42
    ram[0x0003] = 0x42;

    ram[0x0004] = 0x00;  // BRK (stop)
}


