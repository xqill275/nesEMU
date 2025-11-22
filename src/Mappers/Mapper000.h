#ifndef MAPPER000_H
#define MAPPER000_H

#include "mapper.h"

class Mapper000 : public Mapper {
public:
    Mapper000(uint8_t prgBanks, uint8_t chrBanks);
    ~Mapper000() override = default;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) override;
};

#endif
