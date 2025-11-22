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
    ifs.read((char*)header, 16);

    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
        std::cout << "Invalid NES header\n";
        return;
    }

    uint8_t prgBanks = header[4];
    uint8_t chrBanks = header[5];

    mapperID = (header[6] >> 4);

    uint16_t prgSize = prgBanks * 16384;
    uint16_t chrSize = chrBanks * 8192;

    prgRom.resize(prgSize);
    chrRom.resize(chrSize);

    if (header[6] & 0x04) { // skip trainer?
        ifs.seekg(512, std::ios_base::cur);
    }

    ifs.read((char*)prgRom.data(), prgSize);
    ifs.read((char*)chrRom.data(), chrSize);

    ifs.close();

    // Mapper 0 only
    mapper = std::make_shared<Mapper000>(prgBanks, chrBanks);

    valid = true;
}

bool cartridge::cpuRead(uint16_t addr, uint8_t& data)
{
    uint32_t mappedAddr = 0;
    if (mapper->cpuMapRead(addr, mappedAddr)) {
        data = prgRom[mappedAddr];
        return true;
    }
    return false;
}

bool cartridge::cpuWrite(uint16_t addr, uint8_t data)
{
    uint32_t mappedAddr = 0;
    if (mapper->cpuMapWrite(addr, mappedAddr)) {
        prgRom[mappedAddr] = data;  // Rare but allowed if CHR RAM or PRG RAM
        return true;
    }
    return false;
}
