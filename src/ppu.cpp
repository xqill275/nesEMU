#include "header/ppu.h"
#include "header/cartridge.h"
#include <cstdint>

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
    frame_complete = false;
}

void ppu::connectCartridge(cartridge* c) {
    cart = c;
}


// Mirroring helper: map $2000-$2FFF into vram[0..0x07FF]
uint16_t ppu::mapNametableAddr(uint16_t addr) const {
    // Expect addr in $2000-$2FFF or already mirrored $2000-$2EFF
    // Convert to 0..0x0FFF
    addr &= 0x0FFF;

    uint16_t table  = (addr / 0x0400) & 0x03; // 0..3
    uint16_t offset = addr & 0x03FF;

    // Default fold if no cart
    if (!cart) {
        uint16_t page = table & 0x01;
        return (page * 0x0400) + offset;
    }

    uint16_t page = 0;
    switch (cart->mirror) {
        case cartridge::Mirror::VERTICAL:
            // NT0,NT2 -> 0 ; NT1,NT3 -> 1
            page = table & 0x01;
            break;

        case cartridge::Mirror::HORIZONTAL:
            // NT0,NT1 -> 0 ; NT2,NT3 -> 1
            page = (table >> 1) & 0x01;
            break;

        case cartridge::Mirror::FOUR_SCREEN:
            // Can't truly support with only 2KB vram here; best-effort
            page = table & 0x01;
            break;
    }

    return (page * 0x0400) + offset;
}


// CPU <-> PPU registers ($2000-$2007 mirrored)
uint8_t ppu::cpuRead(uint16_t addr, bool readonly) {
    (void)readonly;

    uint8_t data = 0x00;
    addr &= 0x0007;

    switch (addr) {
        case 0x0002: { // PPUSTATUS
            // top 3 bits from status, low 5 bits from read buffer
            data = (PPUSTATUS & 0xE0) | (data_buffer & 0x1F);

            // clear VBlank on read
            PPUSTATUS &= ~0x80;

            // reset latch for $2005/$2006
            addr_latch = 0;
        } break;

        case 0x0004: { // OAMDATA
            data = OAM[OAMADDR];
        } break;

        case 0x0007: { // PPUDATA
            uint16_t a = vram_addr.reg & 0x3FFF;

            // buffered reads except palette
            data = data_buffer;
            data_buffer = ppuRead(a);

            if (a >= 0x3F00)
                data = data_buffer;

            // increment vram addr by 1 or 32
            vram_addr.reg += (PPUCTRL & 0x04) ? 32 : 1;
        } break;

        default:
            break;
    }

    return data;
}

void ppu::cpuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x0007;

    switch (addr) {
        case 0x0000: { // PPUCTRL
            PPUCTRL = data;

            // t: ....BA.. ........ = d: ......BA
            tram_addr.nametable_x = (data & 0x01) != 0;
            tram_addr.nametable_y = (data & 0x02) != 0;
        } break;

        case 0x0001: { // PPUMASK
            PPUMASK = data;
        } break;

        case 0x0003: { // OAMADDR
            OAMADDR = data;
        } break;

        // NOTE: you don't expose $2004 register in ppu.h, but CPU can still write it
        case 0x0004: { // OAMDATA
            OAM[OAMADDR] = data;
            OAMADDR++;
        } break;

        case 0x0005: { // PPUSCROLL (two writes)
            if (addr_latch == 0) {
                // first write: coarse X + fine X
                fine_x = data & 0x07;
                tram_addr.coarse_x = (data >> 3) & 0x1F;
                addr_latch = 1;
            } else {
                // second write: coarse Y + fine Y
                tram_addr.fine_y   = data & 0x07;
                tram_addr.coarse_y = (data >> 3) & 0x1F;
                addr_latch = 0;
            }
        } break;

        case 0x0006: { // PPUADDR (two writes)
            if (addr_latch == 0) {
                // high byte (only 6 bits used)
                tram_addr.reg = (tram_addr.reg & 0x00FF) | ((uint16_t)(data & 0x3F) << 8);
                addr_latch = 1;
            } else {
                // low byte
                tram_addr.reg = (tram_addr.reg & 0xFF00) | data;
                vram_addr = tram_addr;
                addr_latch = 0;
            }
        } break;

        case 0x0007: { // PPUDATA
            uint16_t a = vram_addr.reg & 0x3FFF;
            ppuWrite(a, data);
            vram_addr.reg += (PPUCTRL & 0x04) ? 32 : 1;
        } break;

        default:
            break;
    }
}


// PPU timing
void ppu::clock() {
    cycle++;

    if (cycle >= 341) {
        cycle = 0;
        scanline++;

        // start of vblank
        if (scanline == 241) {
            PPUSTATUS |= 0x80;
            if (PPUCTRL & 0x80)
                nmi = true;
        }

        // pre-render scanline
        if (scanline == 261) {
            // Clear vblank, sprite 0 hit, sprite overflow
            PPUSTATUS &= ~0xE0; // clears bits 5,6,7
            nmi = false;
        }

        // end of frame
        if (scanline >= 262) {
            scanline = 0;
            frame_complete = true;
        }
    }
}


// PPU memory map
uint8_t ppu::ppuRead(uint16_t addr) {
    addr &= 0x3FFF;

    uint8_t data = 0x00;

    // Cartridge first (CHR / mapper)
    if (cart && cart->ppuRead(addr, data))
        return data;

    // Nametable space: $2000-$3EFF (with $3000-$3EFF mirror down)
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        if (addr >= 0x3000) addr -= 0x1000;
        uint16_t idx = mapNametableAddr(addr);
        return vram[idx];
    }

    // Palette: $3F00-$3FFF
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;

        // Mirror $3F10/$3F14/$3F18/$3F1C
        if (addr == 0x10) addr = 0x00;
        if (addr == 0x14) addr = 0x04;
        if (addr == 0x18) addr = 0x08;
        if (addr == 0x1C) addr = 0x0C;

        return palette[addr];
    }

    return 0x00;
}

void ppu::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    // Cartridge first (CHR RAM, mapper writes)
    if (cart && cart->ppuWrite(addr, data))
        return;

    // Nametable: $2000-$3EFF
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        if (addr >= 0x3000) addr -= 0x1000;
        uint16_t idx = mapNametableAddr(addr);
        vram[idx] = data;
        return;
    }

    // Palette: $3F00-$3FFF
    if (addr >= 0x3F00 && addr <= 0x3FFF) {
        addr &= 0x001F;

        if (addr == 0x10) addr = 0x00;
        if (addr == 0x14) addr = 0x04;
        if (addr == 0x18) addr = 0x08;
        if (addr == 0x1C) addr = 0x0C;

        palette[addr] = data;
        return;
    }
}


// Pattern table viewer (for your UI)
void ppu::updatePatternTable() {
    if (!cart) return;

    for (int table = 0; table < 2; table++) {
        for (int tileY = 0; tileY < 16; tileY++) {
            for (int tileX = 0; tileX < 16; tileX++) {

                int tileIndex = tileY * 16 + tileX;
                uint16_t tileAddr = (uint16_t)table * 0x1000 + (uint16_t)tileIndex * 16;

                for (int row = 0; row < 8; row++) {
                    // safest: use ppuRead so CHR-RAM works too
                    uint8_t plane0 = ppuRead(tileAddr + row);
                    uint8_t plane1 = ppuRead(tileAddr + row + 8);

                    for (int col = 0; col < 8; col++) {
                        uint8_t bit0 = (plane0 >> (7 - col)) & 1;
                        uint8_t bit1 = (plane1 >> (7 - col)) & 1;
                        uint8_t pixel = (bit1 << 1) | bit0;

                        // viewer uses palette entries 0..3 as colors
                        uint8_t pal = ppuRead(0x3F00 + pixel) & 0x3F;
                        uint32_t color = nes_colors[pal];

                        int x = tileX * 8 + col;
                        int y = tileY * 8 + row;

                        patternTable[table][y * 128 + x] = color;
                    }
                }
            }
        }
    }
}


// Background renderer (frame-based scrolling using tram_addr/fine_x)
void ppu::renderBackground() {
    uint32_t bgColor = nes_colors[ppuRead(0x3F00) & 0x3F];
    frame.fill(bgColor);

    if (!(PPUMASK & 0x08))
        return;

    // scroll values from tram_addr (what CPU last wrote via $2005/$2006)
    int scrollX = (int)tram_addr.coarse_x * 8 + (int)fine_x;
    int scrollY = (int)tram_addr.coarse_y * 8 + (int)tram_addr.fine_y;

    // base nametable selection from tram_addr
    int baseNTX = tram_addr.nametable_x ? 1 : 0;
    int baseNTY = tram_addr.nametable_y ? 1 : 0;

    uint16_t patternBase = (PPUCTRL & 0x10) ? 0x1000 : 0x0000;

    // Render each pixel as a window into the 2x2 nametable space
    for (int y = 0; y < 240; y++) {
        int worldY = y + scrollY;

        // include base nametable selection vertically (adds 240)
        worldY += baseNTY * 240;

        int ntY = (worldY / 240) & 1;
        int localY = worldY % 240;

        int tileY = localY / 8;
        int fine_y = localY & 7;

        for (int x = 0; x < 256; x++) {
            int worldX = x + scrollX;

            // include base nametable selection horizontally (adds 256)
            worldX += baseNTX * 256;

            int ntX = (worldX / 256) & 1;
            int localX = worldX % 256;

            int tileX = localX / 8;
            int fine_x_pix = localX & 7;

            int ntIndex = ntY * 2 + ntX; // 0..3
            uint16_t nametableBase = 0x2000 + (uint16_t)ntIndex * 0x0400;

            // tile id
            uint16_t ntTileAddr = nametableBase + (uint16_t)tileY * 32 + (uint16_t)tileX;
            uint8_t tileIndex = ppuRead(ntTileAddr);

            // attribute
            uint16_t attrAddr = nametableBase + 0x03C0
                              + (uint16_t)(tileY / 4) * 8
                              + (uint16_t)(tileX / 4);

            uint8_t attrByte = ppuRead(attrAddr);

            int shift = ((tileY & 2) << 1) | (tileX & 2);
            uint8_t palSelect = (attrByte >> shift) & 0x03;

            // pattern fetch
            uint16_t patternAddr = patternBase + (uint16_t)tileIndex * 16 + (uint16_t)fine_y;
            uint8_t plane0 = ppuRead(patternAddr);
            uint8_t plane1 = ppuRead(patternAddr + 8);

            int bit = 7 - fine_x_pix;
            uint8_t b0 = (plane0 >> bit) & 1;
            uint8_t b1 = (plane1 >> bit) & 1;
            uint8_t pixel = (b1 << 1) | b0;

            uint8_t palIndex;
            if (pixel == 0) {
                palIndex = ppuRead(0x3F00) & 0x3F;
            } else {
                palIndex = ppuRead(0x3F00 + palSelect * 4 + pixel) & 0x3F;
            }

            frame[y * 256 + x] = nes_colors[palIndex];
        }
    }
}


// Sprite renderer (supports 8x8 and 8x16)
void ppu::renderSprites() {
    if (!(PPUMASK & 0x10))
        return;

    // Sprite 0 hit only makes sense if BG is enabled too
    bool bg_enabled     = (PPUMASK & 0x08) != 0;
    bool spr_enabled    = (PPUMASK & 0x10) != 0;
    bool bg_left8       = (PPUMASK & 0x02) != 0; // show background in leftmost 8 pixels
    bool spr_left8      = (PPUMASK & 0x04) != 0; // show sprites in leftmost 8 pixels

    bool sprite8x16 = (PPUCTRL & 0x20) != 0;

    // Universal background color (your BG renderer uses this for pixel==0)
    uint32_t bgColor = nes_colors[ppuRead(0x3F00) & 0x3F];

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

        // 8x8 pattern table select
        uint16_t patternBase8x8 = (PPUCTRL & 0x08) ? 0x1000 : 0x0000;

        for (int row = 0; row < spriteHeight; row++) {
            int srcRow = flipV ? (spriteHeight - 1 - row) : row;

            uint16_t tileAddr = 0x0000;

            if (!sprite8x16) {
                tileAddr = patternBase8x8 + (uint16_t)tileIndex * 16;
            } else {
                // 8x16: bank selected by tileIndex bit0
                uint16_t bank = (tileIndex & 0x01) ? 0x1000 : 0x0000;

                // tileIndex selects two stacked tiles (even=top, odd=bottom)
                uint8_t topTile = tileIndex & 0xFE;
                uint8_t useTile = (srcRow < 8) ? topTile : (uint8_t)(topTile + 1);

                uint8_t rowInTile = (uint8_t)(srcRow & 0x07);

                tileAddr = bank + (uint16_t)useTile * 16;
                srcRow = rowInTile;
            }

            uint8_t plane0 = ppuRead(tileAddr + (uint16_t)srcRow);
            uint8_t plane1 = ppuRead(tileAddr + (uint16_t)srcRow + 8);

            for (int col = 0; col < 8; col++) {
                int srcCol = flipH ? col : (7 - col);

                uint8_t bit0  = (plane0 >> srcCol) & 1;
                uint8_t bit1  = (plane1 >> srcCol) & 1;
                uint8_t pixel = (bit1 << 1) | bit0;

                // transparent sprite pixel
                if (pixel == 0)
                    continue;

                int x = (int)spriteX + col;
                int y = baseY + row;

                if (x < 0 || x >= 256 || y < 0 || y >= 240)
                    continue;

                // Leftmost 8 pixel masking rules
                bool in_left8 = (x < 8);
                bool sprite_visible_here = !in_left8 || spr_left8;
                if (!sprite_visible_here)
                    continue;

                // Priority: if behind BG, only draw if BG is universal bg color
                if (behindBG && frame[y * 256 + x] != bgColor)
                    continue;

                // ---------- Sprite 0 Hit ----------
                // Set when sprite #0 overlaps a non-zero BG pixel.
                // We approximate "BG pixel non-zero" by checking it's NOT universal bg color.
                // Also apply left-8 masking: BG must be visible there for hit.
                if (i == 0 && bg_enabled && spr_enabled) {
                    bool bg_visible_here = !in_left8 || bg_left8;

                    if (bg_visible_here) {
                        // Only count a hit if BG pixel is not "color 0"
                        if (frame[y * 256 + x] != bgColor) {
                            // Commonly avoid x==255 quirk; harmless either way,
                            // but this matches typical emulator behavior.
                            if (x != 255) {
                                PPUSTATUS |= 0x40; // sprite 0 hit
                            }
                        }
                    }
                }
                // ----------------------------------

                uint8_t palIndex = ppuRead(0x3F10 + palSel * 4 + pixel) & 0x3F;
                frame[y * 256 + x] = nes_colors[palIndex];
            }
        }
    }
}
