#pragma once
#include "Mapper.h"
#include <cstdint>

class Mapper001 : public Mapper {
public:
    Mapper001(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) override;

    uint8_t getControl() const { return control; }

    // Optional: expose mirroring bits if you want cart->mirror updated dynamically
    // uint8_t getMirrorMode() const { return control & 0x03; }

private:
    // MMC1 internal registers
    uint8_t shiftReg = 0x10; // starts at 0b10000
    uint8_t control  = 0x0C; // default: PRG mode 3, vertical mirroring (common)
    uint8_t chrBank0 = 0x00;
    uint8_t chrBank1 = 0x00;
    uint8_t prgBank  = 0x00;

    // Derived mode bits
    uint8_t mirroring() const { return control & 0x03; }
    uint8_t prgMode()   const { return (control >> 2) & 0x03; }
    uint8_t chrMode()   const { return (control >> 4) & 0x01; }


    void commit(uint16_t addr, uint8_t value);
};
