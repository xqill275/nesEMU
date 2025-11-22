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

    // 1) Cartridge first
    if (cart && cart->cpuRead(addr, data))
        return data;

    // 2) Internal RAM (mirrored)
    if (addr <= 0x1FFF) {
        return ram[addr & 0x07FF];
    }

    // Otherwise - not implemented
    return 0;
}

void bus::write(uint16_t addr, uint8_t data) {

    // 1) Cartridge first
    if (cart && cart->cpuWrite(addr, data))
        return;

    // 2) Internal RAM
    if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
    }
}

void bus::reset() {
    for (auto& r : ram)
        r = 0;
}
