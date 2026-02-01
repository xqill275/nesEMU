#include "Mapper001.h"

Mapper001::Mapper001(uint8_t prgBanks_, uint8_t chrBanks_)
    : Mapper(prgBanks_, chrBanks_) {

    // Power-on defaults (common behavior expected by many games)
    shiftReg = 0x10;
    control  = 0x0C; // PRG mode 3, 16KB switch, last bank fixed
    chrBank0 = 0x00;
    chrBank1 = 0x00;
    prgBank  = 0x00;
}

void Mapper001::commit(uint16_t addr, uint8_t value) {
    // value is 5 bits
    value &= 0x1F;

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        control = value;
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        chrBank0 = value;
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        chrBank1 = value;
    } else if (addr >= 0xE000 && addr <= 0xFFFF) {
        prgBank = value;
    }
}

bool Mapper001::cpuMapRead(uint16_t addr, uint32_t& mappedAddr)
{
    // --- PRG-RAM ($6000-$7FFF), 8KB ---
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = addr & 0x1FFF;   // 8KB window
        return true;
    }

    // --- PRG-ROM ($8000-$FFFF) ---
    if (addr < 0x8000) return false;

    // Safety (should never be 0 for valid carts, but avoids UB)
    const uint32_t prgBankCount16k = (prgBanks == 0) ? 1 : prgBanks;
    const uint32_t lastBank16k = prgBankCount16k - 1;

    const uint16_t offset = addr & 0x3FFF;

    switch (prgMode())
    {
        case 0:
        case 1:
        {
            // 32KB mode: map two consecutive 16KB banks starting at an EVEN bank.
            // MMC1 ignores bit0 here, so use prgBank & 0x0E (even).
            uint32_t bank16k = (prgBank & 0x0E);

            // Clamp to available banks and keep it even (important when bank count is small)
            bank16k %= prgBankCount16k;
            bank16k &= ~1u;

            if (addr < 0xC000) {
                mappedAddr = bank16k * 0x4000 + offset;
            } else {
                uint32_t bank16k_hi = (bank16k + 1) % prgBankCount16k;
                mappedAddr = bank16k_hi * 0x4000 + offset;
            }
        } break;

        case 2:
        {
            // Fix FIRST 16KB at $8000, switch 16KB at $C000
            if (addr < 0xC000) {
                mappedAddr = 0 * 0x4000 + offset;
            } else {
                uint32_t bank = (prgBank & 0x0F) % prgBankCount16k;
                mappedAddr = bank * 0x4000 + offset;
            }
        } break;

        case 3:
        default:
        {
            // Switch 16KB at $8000, fix LAST 16KB at $C000
            if (addr < 0xC000) {
                uint32_t bank = (prgBank & 0x0F) % prgBankCount16k;
                mappedAddr = bank * 0x4000 + offset;
            } else {
                mappedAddr = lastBank16k * 0x4000 + offset;
            }
        } break;
    }

    return true;
}

bool Mapper001::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data)
{
    // --- PRG-RAM ($6000-$7FFF) ---
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = addr & 0x1FFF;
        return true;
    }

    // --- MMC1 registers ($8000-$FFFF) ---
    if (addr < 0x8000) return false;

    // Tell cartridge this is a mapper-register write (not PRG ROM)
    mappedAddr = 0xFFFFFFFF;

    // Reset shift register if bit 7 set
    if (data & 0x80) {
        shiftReg = 0x10;
        control |= 0x0C;   // force PRG mode 3 (common expected behavior)
        return true;
    }

    // Serial load: shift right, new bit enters bit 4
    bool complete = (shiftReg & 0x01) != 0;
    shiftReg >>= 1;
    shiftReg |= (data & 0x01) << 4;

    if (complete) {
        commit(addr, shiftReg & 0x1F);
        shiftReg = 0x10;
    }

    return true;
}


bool Mapper001::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x2000) return false;

    // CHR mapping: either 8KB mode or two 4KB banks
    if (chrBanks == 0) {
        // CHR RAM: direct map
        mappedAddr = addr & 0x1FFF;
        return true;
    }

    if (chrMode() == 0) {
        // 8KB mode: use chrBank0, ignore bit0
        uint32_t bank8k = (chrBank0 & 0x1E);
        mappedAddr = bank8k * 0x1000 + (addr & 0x1FFF);
    } else {
        // 4KB mode
        if (addr < 0x1000) {
            mappedAddr = (chrBank0 & 0x1F) * 0x1000 + (addr & 0x0FFF);
        } else {
            mappedAddr = (chrBank1 & 0x1F) * 0x1000 + (addr & 0x0FFF);
        }
    }

    return true;
}

bool Mapper001::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x2000) return false;

    // Only writable if CHR RAM
    if (chrBanks == 0) {
        mappedAddr = addr & 0x1FFF;
        return true;
    }

    return false;
}
