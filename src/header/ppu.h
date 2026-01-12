#ifndef PPU_H
#define PPU_H

#include <cstdint>
#include <array>

class cartridge;

class ppu {
public:
    ppu();

    // CPU <-> PPU interface ($2000-$2007)
    uint8_t cpuRead(uint16_t addr, bool readonly = false);
    void    cpuWrite(uint16_t addr, uint8_t data);

    // PPU clock (called 3x per CPU clock later)
    void clock();

    // Status
    bool nmi = false;

    // Attach cartridge (CHR ROM/RAM access)
    void connectCartridge(cartridge* cart);

    // PPU registers
    uint8_t PPUCTRL   = 0x00;  // $2000
    uint8_t PPUMASK   = 0x00;  // $2001
    uint8_t PPUSTATUS = 0x00;  // $2002
    uint8_t OAMADDR   = 0x00;  // $2003
    uint8_t OAMDATA   = 0x00;  // $2004
    uint8_t PPUSCROLL = 0x00;  // $2005 (write twice)
    uint8_t PPUADDR   = 0x00;  // $2006 (write twice)
    uint8_t PPUDATA   = 0x00;  // $2007

    // Internal latches
    uint8_t addr_latch = 0;
    uint8_t data_buffer = 0;

    uint16_t vram_addr = 0;
    uint16_t tram_addr = 0;

    // Timing
    int16_t scanline = 0;
    int16_t cycle = 0;

private:
    cartridge* cart = nullptr;


};

#endif