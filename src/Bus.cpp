#include "header/Bus.h"
#include "header/cpu.h"
#include "header/ppu.h"
#include "header/apu.h"
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

void bus::connectAPU(apu *apu) {
    connectedAPU = apu;
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

    if (addr == 0x4015) {
         return connectedAPU->cpuRead(addr, readonly);
    }

    if (addr == 0x4016 || addr == 0x4017) {
        int idx = (addr == 0x4016) ? 0 : 1;

        uint8_t data_out = 0x00;

        // If strobe is high, keep returning current A button state (bit0)
        if (controller_strobe & 0x01) {
            data_out = controller[idx] & 0x01;
        } else {
            // Return lowest bit, then shift right
            data_out = controller_state[idx] & 0x01;
            controller_state[idx] >>= 1;
        }

        return data_out;
    }

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

    if (addr >= 0x4000 && addr <= 0x4017) {
        // $4014 handled earlier (DMA)
        // $4016 handled earlier (controllers)
        if (connectedAPU && addr != 0x4014 && addr != 0x4016 && addr != 0x4017) {
            // Note: we DO want $4017 to go to APU too, so don't exclude it here
        }

        if (connectedAPU && addr != 0x4014 && addr != 0x4016) {
            connectedAPU->cpuWrite(addr, data);
            return;
        }
    }

    if (addr == 0x4016) {
        // When strobe goes from 1 -> 0, latch controllers
        if ((controller_strobe & 0x01) && ((data & 0x01) == 0)) {
            controller_state[0] = controller[0];
            controller_state[1] = controller[1];
        }

        controller_strobe = data & 0x01;

        // If strobe is 1, continuously latch (common behavior)
        if (controller_strobe & 0x01) {
            controller_state[0] = controller[0];
            controller_state[1] = controller[1];
        }

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


void bus::clock()
{
    // PPU always clocks
    connectedPPU->clock();

    // CPU/APU/DMA happen on CPU ticks (1 CPU cycle per 3 PPU cycles)
    if (systemClockCounter % 3 == 0)
    {
        // APU clocks once per CPU cycle (even during DMA)
        if (connectedAPU) connectedAPU->clock();

        if (dma_transfer)
        {
            // DMA dummy cycle: wait until an odd CPU cycle before starting reads/writes
            // Use CPU-cycle parity, not PPU-cycle parity.
            static uint64_t cpuCycleCount = 0; // local static ok, or make a member
            cpuCycleCount++;

            if (dma_dummy)
            {
                // On real hardware DMA begins on an even CPU cycle; many emus do:
                // wait for cpuCycleCount to be odd then start.
                if (cpuCycleCount & 1) {
                    dma_dummy = false;
                }
            }
            else
            {
                // Alternate read/write each CPU cycle
                if ((cpuCycleCount & 1) == 0)
                {
                    // Read from CPU memory
                    uint16_t addr = (uint16_t(dma_page) << 8) | dma_addr;
                    dma_data = read(addr, true);
                }
                else
                {
                    // Write to OAM at current OAMADDR
                    connectedPPU->OAM[connectedPPU->OAMADDR] = dma_data;
                    connectedPPU->OAMADDR++;

                    dma_addr++;
                    if (dma_addr == 0x00) { // wrapped after 256 bytes
                        dma_transfer = false;
                        dma_dummy = true;
                    }
                }
            }

            // CPU core is stalled during DMA (do not clock CPU)
        }
        else
        {
            // Normal CPU cycle
            connectedCPU->clock();
        }
    }

    // Handle NMI (PPU asserts line; CPU samples/handles)
    if (connectedPPU->nmi)
    {
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
    if (connectedAPU) connectedAPU->reset();
}

void bus::setControllerState(int idx, uint8_t state) {
    if (idx < 0 || idx > 1) return;
    controller[idx] = state;
}