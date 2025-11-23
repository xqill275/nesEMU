#ifndef BUS_H
#define BUS_H

#include <cstdint>
#include <array>

class cpu;
class cartridge;

class bus {
public:
    bus();
    ~bus();

    cpu* connectedCPU = nullptr;
    cartridge* cart = nullptr;  // <-- ADD THIS

    std::array<uint8_t, 2048> ram = {};
    std::array<uint8_t, 8> ppu_registers{};

    void connectCpu(cpu* cpu);
    void insertCartridge(cartridge* cart);  // <-- ADD THIS

    uint8_t read(uint16_t addr, bool readonly = false);
    void write(uint16_t addr, uint8_t data);

    void reset();
};
#endif
