//
// Created by olive on 21/11/2025.
//

#include "header/cpu.h"
#include "header/Bus.h"
#include "external/imgui/imgui.h"
#include <iostream>

void cpu::displayRegisters() const {
    std::cout << "A: " << std::hex << +A << "  ";
    std::cout << "X: " << std::hex << +X << "  ";
    std::cout << "Y: " << std::hex << +Y << "  ";
    std::cout << "SP: " << std::hex << +SP << "  ";
    std::cout << "PC: " << std::hex << +PC << "  ";
    std::cout << "P: " << std::hex << +P << " " << std::endl;
}


void cpu::connectBus(bus* bus) {
    this->connectedBus = bus;
}

uint8_t cpu::read(uint16_t addr) {
    return connectedBus->read(addr);
}

void cpu::write(uint16_t addr, uint8_t data) {
    connectedBus->write(addr, data);
}

void cpu::reset() {
    PC = 0x0000;
    SP = 0xFD; // stack pointer
    A = 0; // accumulator
    X = 0; // X register
    Y = 0; // Y register
    P = 0x24; // proccessor status
    // TODO: create clock system

}


// Sets or clears a specific status flag in the CPU's status register (P)
void cpu::setFlag(FLAGS flag, bool value) {
    // If value is true, we want to SET the bit indicated by 'flag'
    if (value) {
        // Bitwise OR sets the bit(s) in P corresponding to 'flag' to 1
        // Example: P = 00010000, flag = 00000010 → result = 00010010
        P |= flag;
    } else {
        // If value is false, we want to CLEAR the bit
        // Bitwise AND with the bitwise NOT of the flag clears the bit(s)
        // ~flag inverts all bits, making the target bit 0
        // Example: P = 00010010, flag = 00000010
        // ~flag = 11111101 → AND → result = 00010000
        P &= ~flag;
    }
}

// Returns true if the flag bit(s) in 'flag' are currently set in P
bool cpu::getFlag(FLAGS flag) const {
    // Bitwise AND isolates the bit(s) of interest.
    // If the result is non-zero, the flag is set.
    //
    // Example: P = 00010010, flag = 00000010
    // P & flag = 00000010 → not zero → return true
    //
    // Example: P = 00010000, flag = 00000010
    // P & flag = 00000000 → zero → return false
    return (P & flag) != 0;
}



void cpu::execute() {
    uint8_t opcode = read(PC);
    auto it = opcodeMap.find(opcode);
    if (it != opcodeMap.end()) {
        (this->*(it->second))();
    } else {
        std::cerr << "Unknown opcode: " << std::hex << int(opcode) << std::endl;
        PC++;
    }
}


void cpu::BRK() {
    PC++; // BRK increments PC before pushing

    // Push PC high byte
    write(0x0100 + SP--, (PC >> 8) & 0xFF);

    // Push PC low byte
    write(0x0100 + SP--, PC & 0xFF);

    // Set B flag before pushing status
    setFlag(BREAK, true);
    setFlag(UNUSED, true); // always set in NES CPU

    // Push processor status
    write(0x0100 + SP--, P);

    // Disable interrupts
    setFlag(INTERRUPT, true);

    // Fetch BRK/IRQ vector
    uint16_t lo = read(0xFFFE);
    uint16_t hi = read(0xFFFF);
    PC = (hi << 8) | lo;
}


void cpu::LDA_Immediate()
{
    uint8_t value = read(PC + 1);
    A = value;

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 2;
}


void cpu::LDA_ZeroPage()
{
    uint8_t address = read(PC + 1);
    A = read(address);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 2;
}


void cpu::LDA_ZeroPageX()
{
    uint8_t base = read(PC + 1);
    uint8_t addr = base + X;     // wraps automatically

    A = read(addr);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 2;
}

void cpu::LDA_absolute()
{
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = (high << 8) | low;

    A = read(addr);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 3;
}


void cpu::LDA_absoluteX()
{
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = ((high << 8) | low) + X;

    A = read(addr);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 3;
}


void cpu::LDA_absoluteY()
{
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = ((high << 8) | low) + Y;

    A = read(addr);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 3;
}


void cpu::LDA_indirectX()
{
    uint8_t zpBase = read(PC + 1);
    uint8_t ptr = zpBase + X;  // wraps 0xFF → 0x00

    uint8_t low  = read(ptr);
    uint8_t high = read((uint8_t)(ptr + 1));

    uint16_t addr = (high << 8) | low;

    A = read(addr);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 2;
}


void cpu::LDA_indirectY()
{
    uint8_t zpAddr = read(PC + 1);

    uint8_t low  = read(zpAddr);
    uint8_t high = read((uint8_t)(zpAddr + 1));

    uint16_t base = (high << 8) | low;
    uint16_t addr = base + Y;

    A = read(addr);

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A & 0x80);

    PC += 2;
}


void cpu::testFillMemorywithstuff() {
    for (int i = 0; i < 1000; i++) {
        write(i, static_cast<uint8_t>(i & 0xFF));
    }
}


// LDX
void cpu::LDX_Immediate() {
    uint8_t value = read(PC + 1);
    X = value;

    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X & 0x80);

    PC += 2;
}
void cpu::LDX_ZeroPage() {
    uint8_t address = read(PC + 1);
    X = read(address);

    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X & 0x80);

    PC += 2;
}
void cpu::LDX_ZeroPageY() {
    uint8_t base = read(PC + 1);
    uint8_t addr = base + Y;     // wraps automatically

    X = read(addr);

    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X & 0x80);

    PC += 2;
}
void cpu::LDX_absolute() {
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = (high << 8) | low;

    X = read(addr);

    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X & 0x80);

    PC += 3;
}
void cpu::LDX_absoluteY() {
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = ((high << 8) | low) + Y;

    X = read(addr);

    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X & 0x80);

    PC += 3;
}

//LDY
void cpu::LDY_Immediate() {
    uint8_t value = read(PC + 1);
    Y = value;

    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y & 0x80);

    PC += 2;
}
void cpu::LDY_ZeroPage() {
    uint8_t address = read(PC + 1);
    Y = read(address);

    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y & 0x80);

    PC += 2;
}
void cpu::LDY_ZeroPageX() {
    uint8_t base = read(PC + 1);
    uint8_t addr = base + X;     // wraps automatically

    Y = read(addr);

    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y & 0x80);

    PC += 2;
}
void cpu::LDY_absolute() {
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = (high << 8) | low;

    Y = read(addr);

    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y & 0x80);

    PC += 3;
}
void cpu::LDY_absoluteX() {
    uint8_t low  = read(PC + 1);
    uint8_t high = read(PC + 2);
    uint16_t addr = ((high << 8) | low) + Y;

    Y = read(addr);

    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y & 0x80);

    PC += 3;
}

// GUI

void cpu::drawFlagsGui() const
{
    auto drawFlag = [&](const char* label, bool value)
    {
        ImVec4 color = value ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)  // green
                             : ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // red
        ImGui::TextColored(color, "%s", label);
        ImGui::SameLine();
    };

    drawFlag("C", getFlag(CARRY));
    drawFlag("Z", getFlag(ZERO));
    drawFlag("I", getFlag(INTERRUPT));
    drawFlag("D", getFlag(DECIMAL));
    drawFlag("B", getFlag(BREAK));
    drawFlag("U", getFlag(UNUSED));
    drawFlag("O", getFlag(OVERFLOW_));
    drawFlag("N", getFlag(NEGATIVE));

    ImGui::NewLine();
}

void cpu::drawStackGui() {
    ImGui::Text("Stack Pointer: %02X", SP);
    ImGui::Separator();

    ImGui::BeginChild("stack_child", ImVec2(0, 300), true);

    for (int i = 0; i < 256; i++) {
        uint16_t addr = 0x0100 + i;

        if (i % 16 == 0)
            ImGui::Text("%04X: ", addr);

        ImGui::SameLine();

        uint8_t val = connectedBus->read(addr);

        if (i == SP) {
            // highlight the active stack location
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%02X ", val);
        } else {
            ImGui::Text("%02X ", val);
        }
    }

    ImGui::EndChild();
}








