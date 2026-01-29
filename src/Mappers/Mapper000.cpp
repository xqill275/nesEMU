#include "mapper000.h"

Mapper000::Mapper000(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks) {}


// CPU READ (0x8000–0xFFFF)
bool Mapper000::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mappedAddr = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}


// CPU WRITE
bool Mapper000::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    (void)data;
    if (addr >= 0x8000 && addr <= 0xFFFF) {
        mappedAddr = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}



// PPU READ (0x0000–0x1FFF CHR ROM/RAM)
bool Mapper000::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x2000) {
        mappedAddr = addr;
        return true;
    }
    return false;
}


// PPU WRITE (only if CHR RAM)
bool Mapper000::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x2000 && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}
