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

    std::vector<uint8_t> prgRom;
    std::vector<uint8_t> chrRom;

    std::vector<uint8_t> prgRam; // 8KB PRG RAM (battery-backed on many carts)

    uint8_t mapperID = 0;
    uint8_t prgBanks = 0;
    uint8_t chrBanks = 0;

    enum class Mirror {
        HORIZONTAL,
        VERTICAL,
        FOUR_SCREEN
    };

    Mirror mirror = Mirror::HORIZONTAL;

    std::shared_ptr<Mapper> mapper;

    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);

    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);
};

#endif
