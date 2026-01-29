// mapper009.cpp
#include "mapper009.h"

Mapper009::Mapper009(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks) {
    // both latches start at FD
    latch0 = 0; // FD
    latch1 = 0; // FD
}

uint32_t Mapper009::mapPrg8k(uint8_t bank, uint16_t addrInWindow) const {
    // 8KB bank size
    const uint32_t bankSize = 0x2000;
    return (uint32_t)bank * bankSize + (addrInWindow & 0x1FFF);
}

uint32_t Mapper009::mapChr4k(uint8_t bank, uint16_t addrInWindow) const {
    // 4KB bank size
    const uint32_t bankSize = 0x1000;
    return (uint32_t)bank * bankSize + (addrInWindow & 0x0FFF);
}

// CPU READ mapping
bool Mapper009::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;

    const uint32_t prgCount8k = prg8kCount();
    if (prgCount8k == 0) return false;

    if (addr <= 0x9FFF) {
        mappedAddr = mapPrg8k((uint8_t)(prgBank8000 % prgCount8k), addr - 0x8000);
        return true;
    }

    if (addr <= 0xBFFF) {
        uint8_t bank = (prgCount8k >= 3) ? (uint8_t)(prgCount8k - 3) : 0;
        mappedAddr = mapPrg8k(bank, addr - 0xA000);
        return true;
    }

    if (addr <= 0xDFFF) {
        uint8_t bank = (prgCount8k >= 2) ? (uint8_t)(prgCount8k - 2) : 0;
        mappedAddr = mapPrg8k(bank, addr - 0xC000);
        return true;
    }

    uint8_t bank = (prgCount8k >= 1) ? (uint8_t)(prgCount8k - 1) : 0;
    mappedAddr = mapPrg8k(bank, addr - 0xE000);
    return true;
}


// CPU WRITE mapping (this mapper uses writes as control regs; still map PRG space like reads)
bool Mapper009::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000) return false;

    mappedAddr = 0xFFFFFFFF; // IMPORTANT: do NOT let cartridge write PRG ROM

    if (addr >= 0xA000 && addr <= 0xAFFF) { prgBank8000 = data & 0x0F; return true; }
    if (addr >= 0xB000 && addr <= 0xBFFF) { chrFD_0000  = data & 0x1F; return true; }
    if (addr >= 0xC000 && addr <= 0xCFFF) { chrFE_0000  = data & 0x1F; return true; }
    if (addr >= 0xD000 && addr <= 0xDFFF) { chrFD_1000  = data & 0x1F; return true; }
    if (addr >= 0xE000 && addr <= 0xEFFF) { chrFE_1000  = data & 0x1F; return true; }

    if (addr >= 0xF000) {
        mirroringOverrideValid = true;
        mirroringHorizontal = (data & 0x01) != 0;
        return true;
    }

    // $8000-$9FFF typically unused for MMC2 regs, but still "handled"
    return true;
}



// PPU READ mapping
bool Mapper009::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x2000) return false;

    const uint32_t chrCount4k = chr4kCount();
    if (chrCount4k == 0) return false;

    if (addr <= 0x0FFF) {
        uint8_t bank = (latch0 == 0) ? chrFD_0000 : chrFE_0000;
        mappedAddr = mapChr4k((uint8_t)(bank % chrCount4k), addr);
    } else {
        uint8_t bank = (latch1 == 0) ? chrFD_1000 : chrFE_1000;
        mappedAddr = mapChr4k((uint8_t)(bank % chrCount4k), addr - 0x1000);
    }

    updateLatchesAfterRead(addr);
    return true;
}

bool Mapper009::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) {
    // MMC2 games are typically CHR ROM, not CHR RAM.
    // If you ever see chrBanks == 0, you can allow CHR RAM writes.
    if (addr < 0x2000 && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}

void Mapper009::updateLatchesAfterRead(uint16_t addr) {
    // Latch triggers (MMC2)
    // $0FD8 -> latch0 = FD
    // $0FE8 -> latch0 = FE
    // $1FD8-$1FDF -> latch1 = FD
    // $1FE8-$1FEF -> latch1 = FE  :contentReference[oaicite:4]{index=4}

    if (addr >= 0x0FD8 && addr <= 0x0FDF) latch0 = 0;        // FD
    else if (addr >= 0x0FE8 && addr <= 0x0FEF) latch0 = 1;   // FE
    else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch1 = 0;   // FD
    else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch1 = 1;   // FE
}
