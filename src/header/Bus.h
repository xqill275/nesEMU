#ifndef BUS_H
#define BUS_H

#include <cstdint>
#include <array>

class cpu;

class bus {
public:
    bus();
    ~bus();


    cpu* connectedCPU = nullptr;


    // 2 KB internal RAM
    std::array<uint8_t, 2048> ram = {};


    // temporary ROM space for testing (32KB)
    std::array<uint8_t, 32768> rom = {};


    void connectCpu(cpu* cpu);
    uint8_t read(uint16_t addr, bool readonly = false);
    void write(uint16_t addr, uint8_t data);
    void reset();
};
#endif // BUS_H