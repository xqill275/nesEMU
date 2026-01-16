#include "header/cartridge.h"
#include "Mappers/Mapper000.h"
#include <fstream>
#include <iostream>

cartridge::cartridge(const std::string& filename)
{
    valid = false;

    std::ifstream ifs(filename, std::ifstream::binary);
    if (!ifs.is_open()) {
        std::cout << "Cartridge open failed: " << filename << "\n";
        return;
    }

    uint8_t header[16];
    ifs.read(reinterpret_cast<char*>(header), 16);

    // Validate iNES header "NES<EOF>"
    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        std::cout << "Invalid NES header\n";
        return;
    }

    // PRG/CHR sizes
    prgBanks = header[4];
    chrBanks = header[5];

    // Mapper ID uses high nibble of header[6] and high nibble of header[7]
    mapperID = ((header[7] & 0xF0) | (header[6] >> 4));

    // Mirroring flags
    bool fourScreen = (header[6] & 0x08) != 0;
    bool vertical   = (header[6] & 0x01) != 0;

    if (fourScreen) mirror = Mirror::FOUR_SCREEN;
    else            mirror = vertical ? Mirror::VERTICAL : Mirror::HORIZONTAL;

    // Skip trainer if present
    if (header[6] & 0x04) {
        ifs.seekg(512, std::ios_base::cur);
    }

    // Allocate PRG/CHR ROM
    uint32_t prgSize = (uint32_t)prgBanks * 16384;
    uint32_t chrSize = (uint32_t)chrBanks * 8192;

    prgRom.resize(prgSize);

    // If chrBanks == 0 => CHR RAM (8KB)
    if (chrBanks == 0) {
        chrRom.resize(8192);
    } else {
        chrRom.resize(chrSize);
    }

    // Read PRG + CHR
    ifs.read(reinterpret_cast<char*>(prgRom.data()), prgSize);

    if (chrBanks > 0) {
        ifs.read(reinterpret_cast<char*>(chrRom.data()), chrSize);
    }

    ifs.close();

    // Mapper selection (you only have Mapper000 so far)
    switch (mapperID) {
        case 0:
            mapper = std::make_shared<Mapper000>(prgBanks, chrBanks);
            break;

        default:
            std::cout << "Unsupported mapper: " << (int)mapperID << "\n";
            return;
    }

    valid = true;
}

bool cartridge::cpuRead(uint16_t addr, uint8_t& data)
{
    uint32_t mappedAddr = 0;
    if (mapper && mapper->cpuMapRead(addr, mappedAddr)) {
        data = prgRom[mappedAddr];
        return true;
    }
    return false;
}

bool cartridge::cpuWrite(uint16_t addr, uint8_t data)
{
    uint32_t mappedAddr = 0;
    if (mapper && mapper->cpuMapWrite(addr, mappedAddr)) {
        // NOTE: For most carts, writes here would go to PRG-RAM (not PRG-ROM).
        // But this keeps your existing behaviour.
        if (mappedAddr < prgRom.size())
            prgRom[mappedAddr] = data;
        return true;
    }
    return false;
}

bool cartridge::ppuRead(uint16_t addr, uint8_t& data)
{
    uint32_t mappedAddr = 0;
    if (mapper && mapper->ppuMapRead(addr, mappedAddr)) {
        if (mappedAddr < chrRom.size()) {
            data = chrRom[mappedAddr];
            return true;
        }
    }
    return false;
}

bool cartridge::ppuWrite(uint16_t addr, uint8_t data)
{
    uint32_t mappedAddr = 0;
    if (mapper && mapper->ppuMapWrite(addr, mappedAddr)) {
        // Only valid if CHR RAM (chrBanks == 0), mapper enforces this
        if (mappedAddr < chrRom.size())
            chrRom[mappedAddr] = data;
        return true;
    }
    return false;
}
