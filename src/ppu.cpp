#include "header/ppu.h"
#include "header/cartridge.h"


static const uint32_t nes_colors[64] = {
    0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088,
    0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
    0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00,
    0xFF00323C, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4,
    0xFF8814B0, 0xFFA01464, 0xFF982220, 0xFF783C00,
    0xFF545A00, 0xFF287200, 0xFF087C00, 0xFF007628,
    0xFF006678, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFECEEEC, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC,
    0xFFE454EC, 0xFFEC58B4, 0xFFEC6A64, 0xFFD48820,
    0xFFA0AA00, 0xFF74C400, 0xFF4CD020, 0xFF38CC6C,
    0xFF38B4CC, 0xFF3C3C3C, 0xFF000000, 0xFF000000,

    0xFFECEEEC, 0xFFA8CCEC, 0xFFBCBCEC, 0xFFD4B2EC,
    0xFFECAEEC, 0xFFECAED4, 0xFFECB4B0, 0xFFE4C490,
    0xFFCCD278, 0xFFB4DE78, 0xFFA8E290, 0xFF98E2B4,
    0xFFA0D6E4, 0xFFA0A2A0, 0xFF000000, 0xFF000000
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

        case 0x0004: // OAMDATA
            data = OAM[OAMADDR];
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

        case 0x0003: // $2003 OAMADDR
            OAMADDR = data;
            break;

        case 0x0004: // $2004 OAMDATA
            OAM[OAMADDR] = data;
            OAMADDR++;
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

void ppu::renderBackground()
{
    // Background enable bit
    if (!(PPUMASK & 0x08)) {
        // Still clear the frame so you don't see old junk
        uint32_t bgColor = nes_colors[palette[0] & 0x3F];
        frame.fill(bgColor);
        return;
    }

    uint16_t patternBase   = (PPUCTRL & 0x10) ? 0x1000 : 0x0000;
    uint16_t nametableBase = 0x2000; // only NT0 for now

    uint32_t bgColor = nes_colors[palette[0] & 0x3F];
    frame.fill(bgColor);

    for (int tileY = 0; tileY < 30; tileY++) {
        for (int tileX = 0; tileX < 32; tileX++) {

            // Name table tile id (0..255)
            uint8_t tileIndex =
                vram[(nametableBase + tileY * 32 + tileX) & 0x07FF];

            // Attribute table byte for this 4x4 tile region
            uint16_t attrAddr =
                nametableBase + 0x03C0 + (tileY / 4) * 8 + (tileX / 4);

            uint8_t attrByte = vram[attrAddr & 0x07FF];

            // Select 2-bit palette for this 2x2 tile quadrant
            int shift = ((tileY & 2) << 1) | (tileX & 2);
            uint8_t paletteSelect = (attrByte >> shift) & 0x03;

            // Tile pattern address
            uint16_t tileAddr = patternBase + tileIndex * 16;

            for (int row = 0; row < 8; row++) {
                uint8_t plane0 = ppuRead(tileAddr + row);
                uint8_t plane1 = ppuRead(tileAddr + row + 8);

                for (int col = 0; col < 8; col++) {
                    int bit = 7 - col;

                    uint8_t bit0 = (plane0 >> bit) & 1;
                    uint8_t bit1 = (plane1 >> bit) & 1;
                    uint8_t pixel = (bit1 << 1) | bit0; // 0..3

                    // Palette index in PPU palette RAM
                    uint8_t palIndex =
                        (pixel == 0)
                            ? palette[0] // universal bg
                            : palette[paletteSelect * 4 + pixel];

                    uint32_t color = nes_colors[palIndex & 0x3F];

                    int x = tileX * 8 + col;
                    int y = tileY * 8 + row;

                    frame[y * 256 + x] = color;
                }
            }
        }
    }
}

void ppu::renderSprites()
{
    // Sprite enable bit
    if (!(PPUMASK & 0x10))
        return;

    bool sprite8x16 = (PPUCTRL & 0x20) != 0;

    // Background "color 0" (for simple priority test)
    uint32_t bgColor = nes_colors[palette[0] & 0x3F];

    for (int i = 0; i < 64; i++) {
        int o = i * 4;

        uint8_t spriteY   = OAM[o + 0];
        uint8_t tileIndex = OAM[o + 1];
        uint8_t attr      = OAM[o + 2];
        uint8_t spriteX   = OAM[o + 3];

        bool flipH    = (attr & 0x40) != 0;
        bool flipV    = (attr & 0x80) != 0;
        bool behindBG = (attr & 0x20) != 0;

        uint8_t palSel = attr & 0x03;

        // NES sprite Y is top-1
        int baseY = (int)spriteY + 1;

        int spriteHeight = sprite8x16 ? 16 : 8;

        // Determine pattern base + tile addressing
        // 8x8: pattern base comes from PPUCTRL bit 3 (0x08)
        // 8x16: pattern base comes from tileIndex bit 0, and tileIndex selects two stacked tiles
        uint16_t patternBase8x8 = (PPUCTRL & 0x08) ? 0x1000 : 0x0000;

        for (int row = 0; row < spriteHeight; row++) {

            int srcRow = flipV ? (spriteHeight - 1 - row) : row;

            uint16_t tileAddr = 0x0000;

            if (!sprite8x16) {
                // ---- 8x8 ----
                tileAddr = patternBase8x8 + (uint16_t)tileIndex * 16;
            } else {
                // ---- 8x16 ----
                // Bank selected by bit0 of tileIndex
                uint16_t bank = (tileIndex & 0x01) ? 0x1000 : 0x0000;

                // Tile number is even for top, odd for bottom
                uint8_t topTile = tileIndex & 0xFE;
                uint8_t useTile = (srcRow < 8) ? topTile : (uint8_t)(topTile + 1);

                // Row within that 8x8 tile
                uint8_t rowInTile = (uint8_t)(srcRow & 0x07);

                tileAddr = bank + (uint16_t)useTile * 16;

                // Override srcRow to be within 0..7 for fetching
                srcRow = rowInTile;
            }

            uint8_t plane0 = ppuRead(tileAddr + (uint16_t)srcRow);
            uint8_t plane1 = ppuRead(tileAddr + (uint16_t)srcRow + 8);

            for (int col = 0; col < 8; col++) {

                int srcCol = flipH ? col : (7 - col);

                uint8_t bit0  = (plane0 >> srcCol) & 1;
                uint8_t bit1  = (plane1 >> srcCol) & 1;
                uint8_t pixel = (bit1 << 1) | bit0;

                if (pixel == 0)
                    continue;

                int x = (int)spriteX + col;
                int y = baseY + row;

                if (x < 0 || x >= 256 || y < 0 || y >= 240)
                    continue;

                // Priority: if behind BG, only draw if BG is color 0
                if (behindBG && frame[y * 256 + x] != bgColor)
                    continue;

                uint8_t palIndex = palette[0x10 + palSel * 4 + pixel] & 0x3F;
                frame[y * 256 + x] = nes_colors[palIndex];
            }
        }
    }
}



