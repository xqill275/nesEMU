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
    if (addr <= 0x1FFF)
        return ram[addr & 0x07FF];

    // PPU registers ($2000-$3FFF mirrored every 8 bytes)
    if (addr >= 0x2000 && addr <= 0x3FFF)
        return connectedPPU->cpuRead(addr & 0x0007, readonly);

    return 0x00;
}

void bus::write(uint16_t addr, uint8_t data) {

    // OAM DMA ($4014)
    // Starts a DMA transfer of 256 bytes from CPU page (data << 8) into OAM
    if (addr == 0x4014) {
        dma_page = data;
        dma_addr = 0x00;
        dma_dummy = true;
        dma_transfer = true;
        return;
    }

    // Cartridge first
    if (cart && cart->cpuWrite(addr, data))
        return;

    // Internal RAM ($0000-$1FFF mirrored)
    if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
        return;
    }

    // PPU registers ($2000-$3FFF mirrored every 8 bytes)
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        connectedPPU->cpuWrite(addr & 0x0007, data);
        return;
    }

    // Ignore everything else for now
}


void bus::clock() {
    // PPU runs every system clock
    connectedPPU->clock();

    // --------------------------------------------
    // DMA steals CPU time but PPU keeps running
    // --------------------------------------------
    if (dma_transfer) {

        // The "dummy" cycle: DMA begins on an even CPU cycle.
        // We approximate this using systemClockCounter parity.
        if (dma_dummy) {
            if (systemClockCounter % 2 == 1) {
                dma_dummy = false;
            }
        } else {
            // On even cycles: read from CPU bus
            if (systemClockCounter % 2 == 0) {
                uint16_t addr = (uint16_t)dma_page << 8 | dma_addr;
                dma_data = read(addr, true);
            }
            // On odd cycles: write into OAM
            else {
                // Real NES writes starting at OAMADDR
                connectedPPU->OAM[connectedPPU->OAMADDR] = dma_data;
                connectedPPU->OAMADDR++;

                dma_addr++;
                if (dma_addr == 0x00) { // wrapped after 256 bytes
                    dma_transfer = false;
                    dma_dummy = true;
                }
            }
        }

        // CPU does NOT clock during DMA
    }
    else {
        // CPU runs every 3rd PPU clock
        if (systemClockCounter % 3 == 0) {
            connectedCPU->clock();
        }
    }

    // Handle NMI
    if (connectedPPU->nmi) {
        connectedPPU->nmi = false;
        connectedCPU->nmi();
    }

    systemClockCounter++;
}

void bus::reset() {
    for (auto& r : ram) r = 0x00;
    systemClockCounter = 0;

    dma_transfer = false;
    dma_dummy = true;
    dma_page = 0x00;
    dma_addr = 0x00;
    dma_data = 0x00;

    if (connectedCPU) connectedCPU->reset();
}
