#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

class Mapper;   // forward declaration

class cartridge {
public:
    cartridge(const std::string& filename);

    bool valid = false;

    // PRG & CHR data
    std::vector<uint8_t> prgRom;
    std::vector<uint8_t> chrRom;

    uint8_t mapperID = 0;
    uint8_t prgBanks = 0;
    uint8_t chrBanks = 0;

    // Mapper object
    std::shared_ptr<Mapper> mapper;

    // CPU interface
    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);

    // PPU interface (we will implement later)
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);
};

#endif
