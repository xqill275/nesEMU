#include "header/Bus.h"
#include "header/cpu.h"
#include "header/ppu.h"
#include "header/cartridge.h"

bus::bus() {
    reset();
}

bus::~bus() {}

void bus::connectCpu(cpu* cpu) {
    connectedCPU = cpu;
}

void bus::connectPPU(ppu* ppu) {
    connectedPPU = ppu;
}

void bus::insertCartridge(cartridge* cart) {
    this->cart = cart;
    if (connectedPPU)
        connectedPPU->connectCartridge(cart);
}

uint8_t bus::read(uint16_t addr, bool readonly) {
    uint8_t data = 0x00;

    // Cartridge takes priority
    if (cart && cart->cpuRead(addr, data))
        return data;

    // Internal RAM ($0000-$1FFF mirrored)
    if (addr <= 0x1FFF) {
        return ram[addr & 0x07FF];
    }

    // PPU registers ($2000-$3FFF mirrored every 8 bytes)
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        return connectedPPU->cpuRead(addr & 0x0007, readonly);
    }

    // APU / I/O not implemented yet
    return 0x00;
}

void bus::write(uint16_t addr, uint8_t data) {

    // Cartridge first
    if (cart && cart->cpuWrite(addr, data))
        return;

    // Internal RAM
    if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
        return;
    }

    // PPU registers
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        connectedPPU->cpuWrite(addr & 0x0007, data);
        return;
    }

    // Ignore everything else for now
}

void bus::clock() {
    // PPU runs every system clock
    connectedPPU->clock();

    // CPU runs every 3rd PPU clock
    if (systemClockCounter % 3 == 0) {
        connectedCPU->clock();
    }

    // Handle NMI
    if (connectedPPU->nmi) {
        connectedPPU->nmi = false;
        connectedCPU->nmi();
    }

    systemClockCounter++;
}

void bus::reset() {
    for (auto& r : ram)
        r = 0x00;

    systemClockCounter = 0;

    if (connectedCPU) connectedCPU->reset();
}
