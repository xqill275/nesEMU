#ifndef BUS_H
#define BUS_H

#include <cstdint>
#include <array>

class cpu;

class bus {
public:
    bus();
    ~bus();

    // The bus connects to a CPU.
    cpu* connectedCPU = nullptr;

    // 2 KB internal RAM like NES
    std::array<uint8_t, 2048> ram = {};

    // CPU read/write interface
    void connectCpu(cpu* cpu);
    uint8_t read(uint16_t addr, bool readonly = false);
    void write(uint16_t addr, uint8_t data);

    // Reset system memory
    void reset();
};

#endif