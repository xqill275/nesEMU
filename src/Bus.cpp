#include "header/Bus.h"
#include "header/cpu.h"
#include "header/cartridge.h"

bus::bus() {
    reset();
}

bus::~bus() {}

void bus::connectCpu(cpu* cpu) {
    this->connectedCPU = cpu;
}

void bus::insertCartridge(cartridge* cart) {
    this->cart = cart;
}

uint8_t bus::read(uint16_t addr, bool readonly) {
    uint8_t data = 0;

    if (cart && cart->cpuRead(addr, data))
        return data;

    if (addr <= 0x1FFF)
        return ram[addr & 0x07FF];

    // PPU registers ($2000-$2007)
    if (addr >= 0x2000 && addr <= 0x2007)
        return ppu_registers[addr - 0x2000];

    return 0;
}

void bus::write(uint16_t addr, uint8_t data) {

    if (cart && cart->cpuWrite(addr, data))
        return;

    // internal RAM
    if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
    }

    // PPU registers ($2000-$2007)
    else if (addr >= 0x2000 && addr <= 0x2007) {
        ppu_registers[addr - 0x2000] = data;
    }

    // ignore everything else for now
}

void bus::reset() {
    for (auto& r : ram)
        r = 0;
}
