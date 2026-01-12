#include "header/ppu.h"
#include "header/cartridge.h"

ppu::ppu() {}

void ppu::connectCartridge(cartridge* c) {
    cart = c;
}

uint8_t ppu::cpuRead(uint16_t addr, bool readonly) {
    uint8_t data = 0x00;
    addr &= 0x0007;

    switch (addr) {
        case 0x0002: // PPUSTATUS
            data = (PPUSTATUS & 0xE0) | (data_buffer & 0x1F);
            PPUSTATUS &= ~0x80;      // clear VBlank
            addr_latch = 0;
            break;

        case 0x0007: // PPUDATA
            data = data_buffer;
            // No real VRAM yet — just return buffer
            break;

        default:
            break;
    }

    return data;
}

void ppu::cpuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x0007;

    switch (addr) {
        case 0x0000: // PPUCTRL
            PPUCTRL = data;
            tram_addr = (tram_addr & 0xF3FF) | ((data & 0x03) << 10);
            break;

        case 0x0001: // PPUMASK
            PPUMASK = data;
            break;

        case 0x0005: // PPUSCROLL
            // Ignored for now
            break;

        case 0x0006: // PPUADDR
            if (addr_latch == 0) {
                tram_addr = (tram_addr & 0x00FF) | ((data & 0x3F) << 8);
                addr_latch = 1;
            } else {
                tram_addr = (tram_addr & 0xFF00) | data;
                vram_addr = tram_addr;
                addr_latch = 0;
            }
            break;

        case 0x0007: // PPUDATA
            // Writes ignored for now
            break;

        default:
            break;
    }
}

void ppu::clock() {

    cycle++;

    if (cycle >= 341) {
        cycle = 0;
        scanline++;

        // Start of VBlank
        if (scanline == 241) {
            PPUSTATUS |= 0x80;
            if (PPUCTRL & 0x80)
                nmi = true;
        }

        // Pre-render scanline
        if (scanline == 261) {
            PPUSTATUS &= ~0x80;
            nmi = false;
        }

        // End of frame
        if (scanline >= 262) {
            scanline = 0;
        }
    }
}
