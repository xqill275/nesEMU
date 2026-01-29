#include "mapper002.h"

Mapper002::Mapper002(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks) {}

bool Mapper002::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        // switchable 16KB bank
        uint32_t bank = (prgBankSelect % prgBanks);
        mappedAddr = bank * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xFFFF) {
        // fixed last 16KB bank
        uint32_t bank = (prgBanks - 1);
        mappedAddr = bank * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    return false;
}

bool Mapper002::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    (void)mappedAddr;

    if (addr >= 0x8000 && addr <= 0xFFFF) {
        // UxROM: any write selects bank
        prgBankSelect = data & 0x0F; // common mask
        return false; // no ROM write
    }

    return false;
}

bool Mapper002::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x2000) {
        mappedAddr = addr; // CHR fixed
        return true;
    }
    return false;
}

bool Mapper002::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) {
    // Only allow writes if CHR RAM
    if (addr < 0x2000 && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}
