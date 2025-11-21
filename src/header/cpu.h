//
// Created by olive on 21/11/2025.
//

#ifndef CPU_H
#define CPU_H
#include <cstdint>
#include <map>
#include <array>

class bus;


class cpu {
public:

    uint16_t PC = 0; // program counter
    uint8_t SP = 0; // stack pointer
    uint8_t A = 0; // accumulator
    uint8_t X = 0; // X register
    uint8_t Y = 0; // Y register
    uint8_t P = 0x24; // proccessor status



    enum FLAGS {
        CARRY = (1 << 0),
        ZERO = (1 << 1),
        INTERRUPT = (1 << 2),
        DECIMAL = (1 << 3),
        BREAK = (1 << 4),
        UNUSED = (1 << 5),
        OVERFLOW_ = (1 << 6),
        NEGATIVE = (1 << 7)
    };
    bus* connectedBus = nullptr;
    void connectBus(bus* bus);
    void displayRegisters() const;
    void setFlag(FLAGS flag, bool value);
    bool getFlag(FLAGS flag) const;
    void execute();
    void reset();

    void testFillMemorywithstuff();

    uint16_t getPC();

    // BUS ACCSESS
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);

    // GUI STUFF:
    void drawStackGui();
    void drawFlagsGui() const;


private:
    std::map<uint8_t, void(cpu::*)()> opcodeMap = {
        {0x00, &cpu::BRK},
        // --- LDA ---
        {0xA9, &cpu::LDA_Immediate},   // Immediate
        {0xA5, &cpu::LDA_ZeroPage},    // Zero Page
        {0xB5, &cpu::LDA_ZeroPageX},   // Zero Page,X
        {0xAD, &cpu::LDA_absolute},    // Absolute
        {0xBD, &cpu::LDA_absoluteX},   // Absolute,X
        {0xB9, &cpu::LDA_absoluteY},   // Absolute,Y
        {0xA1, &cpu::LDA_indirectX},   // (Indirect,X)
        {0xB1, &cpu::LDA_indirectY},   // (Indirect),Y

        // --- LDX ---
        {0xA2, &cpu::LDX_Immediate},
        {0xA6, &cpu::LDX_ZeroPage},
        {0xB6, &cpu::LDX_ZeroPageY},
        {0xAE, &cpu::LDX_absolute},
        {0xBE, &cpu::LDX_absoluteY},

        // --- LDY ---
        {0xA0, &cpu::LDY_Immediate},
        {0xA4, &cpu::LDY_ZeroPage},
        {0xB4, &cpu::LDY_ZeroPageX},
        {0xAC, &cpu::LDY_absolute},
        {0xBC, &cpu::LDY_absoluteX},

    };

    //BRK
    void BRK();

    // LDA
    void LDA_Immediate();
    void LDA_ZeroPage();
    void LDA_ZeroPageX();
    void LDA_absolute();
    void LDA_absoluteX();
    void LDA_absoluteY();
    void LDA_indirectX();
    void LDA_indirectY();

    //LDX
    void LDX_Immediate();
    void LDX_ZeroPage();
    void LDX_ZeroPageY();
    void LDX_absolute();
    void LDX_absoluteY();

    //LDY
    void LDY_Immediate();
    void LDY_ZeroPage();
    void LDY_ZeroPageX();
    void LDY_absolute();
    void LDY_absoluteX();







};



#endif //CPU_H
