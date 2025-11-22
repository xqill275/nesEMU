#ifndef MAPPER_H
#define MAPPER_H

#include <cstdint>

class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks)
        : prgBanks(prgBanks), chrBanks(chrBanks) {}

    virtual ~Mapper() = default;

    // CPU mapping
    virtual bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr) = 0;

    // PPU mapping (CHR)
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) = 0;

protected:
    uint8_t prgBanks = 0;
    uint8_t chrBanks = 0;
};

#endif
