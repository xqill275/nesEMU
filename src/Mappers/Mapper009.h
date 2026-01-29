// mapper009.h
#pragma once
#include "mapper.h"
#include <cstdint>

class Mapper009 : public Mapper {
public:
    Mapper009(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) override;

    // Optional helper so cartridge can query mirroring override after mapper writes.
    bool hasMirroringOverride() const { return mirroringOverrideValid; }
    bool mirroringIsHorizontal() const { return mirroringHorizontal; }

    uint32_t prg8kCount() const { return (uint32_t)prgBanks * 2u; }   // 16KB -> 8KB
    uint32_t chr4kCount() const { return (uint32_t)chrBanks * 2u; }   // 8KB  -> 4KB


private:
    // PRG
    uint8_t prgBank8000 = 0;

    // CHR regs (4KB banks)
    uint8_t chrFD_0000 = 0;
    uint8_t chrFE_0000 = 0;
    uint8_t chrFD_1000 = 0;
    uint8_t chrFE_1000 = 0;

    // Latches (0 = $FD, 1 = $FE)
    uint8_t latch0 = 0; // start FD
    uint8_t latch1 = 0; // start FD

    // Mirroring written by $F000-$FFFF
    bool mirroringOverrideValid = false;
    bool mirroringHorizontal = false;

private:
    uint32_t mapPrg8k(uint8_t bank, uint16_t addrInWindow) const;
    uint32_t mapChr4k(uint8_t bank, uint16_t addrInWindow) const;

    void updateLatchesAfterRead(uint16_t addr);
};
