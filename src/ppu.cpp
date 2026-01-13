#include "header/ppu.h"
#include "header/cartridge.h"


static const uint32_t nes_colors[64] = {
    0xFF757575, 0xFF271B8F, 0xFF0000AB, 0xFF47009F,
    0xFF8F0077, 0xFFAB0013, 0xFFA70000, 0xFF7F0B00,
    0xFF432F00, 0xFF004700, 0xFF005100, 0xFF003F17,
    0xFF1B3F5F, 0xFF000000, 0xFF000000, 0xFF000000,
    // (you can fill full table later — start with this)
};

ppu::ppu() {
    patternTable[0].resize(128 * 128);
    patternTable[1].resize(128 * 128);
}

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

        case 0x0007:
        {
            uint16_t addr = vram_addr & 0x3FFF;

            data = data_buffer;
            data_buffer = ppuRead(addr);

            // No buffering for palette reads (later)
            if (addr >= 0x3F00)
                data = data_buffer;

            vram_addr += (PPUCTRL & 0x04) ? 32 : 1;
        }
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

        case 0x0007:
        {
            uint16_t addr = vram_addr & 0x3FFF;
            ppuWrite(addr, data);
            vram_addr += (PPUCTRL & 0x04) ? 32 : 1;
        }
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

uint8_t ppu::ppuRead(uint16_t addr) {
    addr &= 0x3FFF;

    uint8_t data = 0x00;

    if (cart && cart->ppuRead(addr, data))
        return data;

    if (addr >= 0x2000 && addr <= 0x2FFF) {
        data = vram[addr & 0x07FF];
    }
    else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;

        // Mirror $3F10/$3F14/$3F18/$3F1C
        if (addr == 0x10) addr = 0x00;
        if (addr == 0x14) addr = 0x04;
        if (addr == 0x18) addr = 0x08;
        if (addr == 0x1C) addr = 0x0C;

        data = palette[addr];
    }

    return data;
}

void ppu::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (cart && cart->ppuWrite(addr, data))
        return;

    if (addr >= 0x2000 && addr <= 0x2FFF) {
        vram[addr & 0x07FF] = data;
    }
    else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;

        if (addr == 0x10) addr = 0x00;
        if (addr == 0x14) addr = 0x04;
        if (addr == 0x18) addr = 0x08;
        if (addr == 0x1C) addr = 0x0C;

        palette[addr] = data;
    }
}

void ppu::updatePatternTable()
{
    if (!cart) return;

    for (int table = 0; table < 2; table++) {
        for (int tileY = 0; tileY < 16; tileY++) {
            for (int tileX = 0; tileX < 16; tileX++) {

                int tileIndex = tileY * 16 + tileX;
                uint16_t tileAddr = table * 0x1000 + tileIndex * 16;

                for (int row = 0; row < 8; row++) {
                    uint8_t plane0 = cart->chrRom[tileAddr + row];
                    uint8_t plane1 = cart->chrRom[tileAddr + row + 8];

                    for (int col = 0; col < 8; col++) {
                        uint8_t bit0 = (plane0 >> (7 - col)) & 1;
                        uint8_t bit1 = (plane1 >> (7 - col)) & 1;
                        uint8_t pixel = (bit1 << 1) | bit0;   // 0–3

                        // --- NES palette lookup ---
                        // Pattern tables use background palette 0 by default
                        uint8_t paletteIndex = palette[pixel];
                        uint32_t color = nes_colors[paletteIndex & 0x3F];

                        int x = tileX * 8 + col;
                        int y = tileY * 8 + row;

                        patternTable[table][y * 128 + x] = color;
                    }
                }
            }
        }
    }
}
