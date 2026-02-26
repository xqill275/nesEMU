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

    void ppu_prefetch_bg_tiles_for_mmc2(ppu* self, int y, int scrollX, int scrollY,
                                        int baseNTX, int baseNTY, uint16_t patternBase);

    void renderBackground();
    void renderSprites();

    bool nmi = false;

    void connectCartridge(cartridge* cart);

    bool bgPixelNonZeroAt(int x, int y);
    bool sprite0PixelNonZeroAt(int x, int y);

    // PPU memory
    std::array<uint8_t, 2048> vram{};
    std::array<uint8_t, 32>   palette{};
    std::array<uint32_t, 256 * 240> frame{};
    std::array<uint8_t, 256>  OAM{};
    std::vector<uint32_t> patternTable[2];

    // Debug per-scanline snapshot state (for frame-based renderer)
    std::array<int, 240> dbg_scrollX{};
    std::array<int, 240> dbg_scrollY{};
    std::array<int, 240> dbg_baseNTX{};
    std::array<int, 240> dbg_baseNTY{};

    // NEW: per-scanline pattern/sprite mode snapshots
    std::array<uint16_t, 240> dbg_bgPatternBase{};
    std::array<uint16_t, 240> dbg_sprPatternBase{};
    std::array<bool, 240>     dbg_sprite8x16{};

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

    bool frame_complete = false;

    bool sprite0_hit_pending = false;
    int  sprite0_hit_x = -1;
    int  sprite0_hit_y = -1;

private:
    cartridge* cart = nullptr;

    uint16_t mapNametableAddr(uint16_t addr) const;

    // helpers for snapshots
    inline uint16_t bgPatternBaseForScanline(int y) const {
        if (y < 0 || y >= 240) return (PPUCTRL & 0x10) ? 0x1000 : 0x0000;
        return dbg_bgPatternBase[y];
    }
    inline uint16_t sprPatternBaseForScanline(int y) const {
        if (y < 0 || y >= 240) return (PPUCTRL & 0x08) ? 0x1000 : 0x0000;
        return dbg_sprPatternBase[y];
    }
    inline bool sprite8x16ForScanline(int y) const {
        if (y < 0 || y >= 240) return (PPUCTRL & 0x20) != 0;
        return dbg_sprite8x16[y];
    }
};

#endif