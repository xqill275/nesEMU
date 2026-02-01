#include "header/cartridge.h"
#include "Mappers/Mapper000.h"
#include "Mappers/Mapper002.h"
#include "Mappers/Mapper009.h"
#include "Mappers/Mapper001.h"
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

    prgRam.resize(8192, 0x00);

    // Read PRG + CHR
    ifs.read(reinterpret_cast<char*>(prgRom.data()), prgSize);

    if (chrBanks > 0) {
        ifs.read(reinterpret_cast<char*>(chrRom.data()), chrSize);
    }

    ifs.close();

    // Mapper selection
    switch (mapperID) {
        case 0:
            mapper = std::make_shared<Mapper000>(prgBanks, chrBanks);
            break;
        case 1:
            mapper = std::make_shared<Mapper001>(prgBanks, chrBanks);
            break;
        case 2:
            mapper = std::make_shared<Mapper002>(prgBanks, chrBanks);
            break;
        case 9:
            mapper = std::make_shared<Mapper009>(prgBanks, chrBanks);
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

        // PRG-RAM region (MMC1 etc.)
        if (addr >= 0x6000 && addr <= 0x7FFF) {
            data = prgRam[mappedAddr & 0x1FFF];
            return true;
        }

        data = prgRom[mappedAddr];
        return true;
    }
    return false;
}

bool cartridge::cpuWrite(uint16_t addr, uint8_t data)
{
    uint32_t mappedAddr = 0;
    if (mapper && mapper->cpuMapWrite(addr, mappedAddr, data)) {

        // Apply MMC2 mirroring override if present
        if (mapperID == 9) {
            auto* m9 = dynamic_cast<Mapper009*>(mapper.get());
            if (m9 && m9->hasMirroringOverride()) {
                mirror = m9->mirroringIsHorizontal() ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            }
        }

        if (mapperID == 1) {
            auto* m1 = dynamic_cast<Mapper001*>(mapper.get());
            if (m1) {
                // MMC1 mirroring bits: 0,1 one-screen; 2 vertical; 3 horizontal
                // Your cart enum only supports H/V/4-screen, so:
                uint8_t mir = (m1->getControl() & 0x03); // if you expose getControl()
                if (mir == 2) mirror = Mirror::VERTICAL;
                else if (mir == 3) mirror = Mirror::HORIZONTAL;
                // one-screen modes: pick either, most emus map them specially; you can treat as vertical for now
                else mirror = Mirror::VERTICAL;
            }
        }

        if (mappedAddr == 0xFFFFFFFF)
            return true;

        if (addr >= 0x6000 && addr <= 0x7FFF) {
            prgRam[mappedAddr & 0x1FFF] = data;
            return true;
        }

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
