#ifndef BUS_H
#define BUS_H

#include <cstdint>
#include <array>

class cpu;
class ppu;
class cartridge;

class bus {
public:
    bus();
    ~bus();

    // Devices
    cpu* connectedCPU = nullptr;
    ppu* connectedPPU = nullptr;
    cartridge* cart   = nullptr;

    // 2KB internal RAM ($0000-$07FF, mirrored)
    std::array<uint8_t, 2048> ram{};

    // Controller
    uint8_t controller[2] = { 0x00, 0x00 };       // live button state (set by your main loop)
    uint8_t controller_state[2] = { 0x00, 0x00 }; // latched/shifted state used by $4016/$4017
    uint8_t controller_strobe = 0x00;

    // Connections
    void connectCpu(cpu* cpu);
    void connectPPU(ppu* ppu);
    void insertCartridge(cartridge* cart);

    void setControllerState(int idx, uint8_t state);

    // CPU bus interface
    uint8_t read(uint16_t addr, bool readonly = false);
    void    write(uint16_t addr, uint8_t data);

    // Master clock
    void clock();

    void reset();

private:
    uint64_t systemClockCounter = 0;

    bool     dma_transfer = false;
    bool     dma_dummy    = true;
    uint8_t  dma_page     = 0x00;
    uint8_t  dma_addr     = 0x00;
    uint8_t  dma_data     = 0x00;
};

#endif
