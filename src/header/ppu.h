#ifndef PPU_H
#define PPU_H

#include <cstdint>
#include <array>
#include <vector>

class cartridge;

class ppu {
public:
    ppu();

    uint8_t cpuRead(uint16_t addr, bool readonly = false);
    void    cpuWrite(uint16_t addr, uint8_t data);

    uint8_t ppuRead(uint16_t addr);
    void    ppuWrite(uint16_t addr, uint8_t data);

    void updatePatternTable();
    void clock();

    void renderBackground();
    void renderSprites();

    bool nmi = false;

    void connectCartridge(cartridge* cart);

    std::array<uint8_t, 2048> vram{};
    std::array<uint8_t, 32>   palette{};
    std::array<uint32_t, 256 * 240> frame{};
    std::array<uint8_t, 256>  OAM{};
    std::vector<uint32_t> patternTable[2];

    // PPU registers
    uint8_t PPUCTRL   = 0x00;  // $2000
    uint8_t PPUMASK   = 0x00;  // $2001
    uint8_t PPUSTATUS = 0x00;  // $2002
    uint8_t OAMADDR   = 0x00;  // $2003

    // Internal latches/buffers
    uint8_t  addr_latch  = 0;     // toggles $2005/$2006
    uint8_t  data_buffer = 0;     // $2007 read buffer
    uint8_t  fine_x      = 0;     // fine X scroll (0..7)

    // "Loopy" VRAM address registers
    union loopy_register {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0x0000;
    };

    loopy_register vram_addr; // current VRAM address (15 bits)
    loopy_register tram_addr; // temporary VRAM address

    // Timing
    int16_t scanline = 0;
    int16_t cycle    = 0;

   bool frame_complete;

private:
    cartridge* cart = nullptr;

    // Map $2000-$2FFF to your 2KB vram[] using cart mirroring
    uint16_t mapNametableAddr(uint16_t addr) const;
};

#endif
