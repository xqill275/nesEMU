// src/cpu.cpp
#include "header/cpu.h"
#include "header/Bus.h"
#include <iostream>
#include "external/imgui/imgui.h"

cpu::cpu() {
    buildLookup();
}

void cpu::connectBus(bus* b) { bus_ptr = b; }

uint8_t cpu::read(uint16_t addr) { return bus_ptr->read(addr); }
void cpu::write(uint16_t addr, uint8_t data) { bus_ptr->write(addr, data); }

inline void cpu::setFlag(FLAGS f, bool v) { if (v) P |= f; else P &= ~f; }
inline bool cpu::getFlag(FLAGS f) const { return (P & f) != 0; }

inline void cpu::setZN(uint8_t v) { setFlag(Z, v == 0); setFlag(N, (v & 0x80) != 0); }

void cpu::push(uint8_t v) {
    write(0x0100 + SP, v);
    SP--;
}

uint8_t cpu::pop() {
    SP++;
    return read(0x0100 + SP);
}

// addressing modes
uint8_t cpu::IMP() { fetched = A; return 0; }
uint8_t cpu::IMM() { addr_abs = PC + 1; return 0; }
uint8_t cpu::ZP0() { addr_abs = read(PC + 1); addr_abs &= 0x00FF; return 0; }
uint8_t cpu::ZPX() { addr_abs = (uint8_t)(read(PC + 1) + X); addr_abs &= 0x00FF; return 0; }
uint8_t cpu::ZPY() { addr_abs = (uint8_t)(read(PC + 1) + Y); addr_abs &= 0x00FF; return 0; }
uint8_t cpu::ABS() { uint16_t lo = read(PC+1); uint16_t hi = read(PC+2); addr_abs = (hi<<8)|lo; return 0; }
uint8_t cpu::ABX() { uint16_t lo = read(PC+1); uint16_t hi = read(PC+2); uint16_t base = (hi<<8)|lo; addr_abs = base + X; return ((base & 0xFF00) != (addr_abs & 0xFF00)); }
uint8_t cpu::ABY() { uint16_t lo = read(PC+1); uint16_t hi = read(PC+2); uint16_t base = (hi<<8)|lo; addr_abs = base + Y; return ((base & 0xFF00) != (addr_abs & 0xFF00)); }

uint8_t cpu::IND() {
    uint16_t ptr_lo = read(PC+1);
    uint16_t ptr_hi = read(PC+2);
    uint16_t ptr = (ptr_hi<<8)|ptr_lo;
    // emulate page-boundary hardware bug
    uint16_t lo = read(ptr);
    uint16_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    addr_abs = (hi<<8) | lo;
    return 0;
}

uint8_t cpu::IZX() {
    uint16_t t = (uint8_t)(read(PC+1) + X);
    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);
    addr_abs = (hi<<8) | lo;
    return 0;
}

uint8_t cpu::IZY() {
    uint16_t t = read(PC+1);
    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);
    addr_abs = ((hi<<8) | lo) + Y;
    uint16_t base = (hi<<8) | lo;
    return ((base & 0xFF00) != (addr_abs & 0xFF00));
}

uint8_t cpu::REL() { addr_rel = (int8_t)read(PC+1); return 0; }

// Operations
uint8_t cpu::XXX() {
    std::cout << "Unknown opcode: $"
              << std::hex << (int)opcode
              << " at PC $"
              << std::hex << PC << "\n";
    return 0;
}

uint8_t cpu::NOP() {
    // Official NOP does nothing; some NOP opcodes have extra cycles but that's fine for now
    return 0;
}

uint8_t cpu::BRK() {
    // BRK behaviour: push PC+1 (PC already points to BRK), push status with B set, set I, load vector
    // many implementations increment PC by 2, but the important behaviour is push PC+1 then vector load.
    PC += 1; // increment to point after BRK (6502 quirk)
    push((PC >> 8) & 0xFF);
    push(PC & 0xFF);
    setFlag(B, true);
    setFlag(U, true);
    push(P);
    setFlag(I, true);
    uint16_t lo = read(0xFFFE);
    uint16_t hi = read(0xFFFF);
    PC = (hi << 8) | lo;
    return 0;
}

uint8_t cpu::ORA() {
    A = A | fetched;
    setZN(A);
    return 0;
}

uint8_t cpu::LDA() {
    // fetched from addr_abs or immediate
    fetched = read(addr_abs);
    A = fetched;
    setZN(A);
    return 0;
}

uint8_t cpu::LDX() {
    fetched = read(addr_abs);
    X = fetched;
    setZN(X);
    return 0;
}

uint8_t cpu::LDY() {
    fetched = read(addr_abs);
    Y = fetched;
    setZN(Y);
    return 0;
}

uint8_t cpu::STA() {
    write(addr_abs, A);
    return 0;
}

uint8_t cpu::TAX() { X = A; setZN(X); return 0; }
uint8_t cpu::TAY() { Y = A; setZN(Y); return 0; }
uint8_t cpu::TXA() { A = X; setZN(A); return 0; }
uint8_t cpu::TYA() { A = Y; setZN(A); return 0; }

uint8_t cpu::TXS() { SP = X; return 0; };


uint8_t cpu::INX() { X++; setZN(X); return 0; }
uint8_t cpu::INY() { Y++; setZN(Y); return 0; }
uint8_t cpu::DEX() { X--; setZN(X); return 0; }
uint8_t cpu::DEY() { Y--; setZN(Y); return 0; }

uint8_t cpu::JMP() {
    uint16_t lo = read(PC+1);
    uint16_t hi = read(PC+2);
    PC = (hi<<8) | lo;
    return 0;
}

uint8_t cpu::JSR() {
    uint16_t return_addr = PC + 2;
    push((return_addr >> 8) & 0xFF);
    push(return_addr & 0xFF);
    {
        uint16_t lo = read(PC+1);
        uint16_t hi = read(PC+2);
        PC = (hi<<8) | lo;
    }
    return 0;
}

uint8_t cpu::RTS() {
    uint16_t lo = pop();
    uint16_t hi = pop();
    PC = (hi<<8) | lo;
    PC++;
    return 0;
}

// set clear

uint8_t cpu::SEI() {
    setFlag(I, true);// Set Interrupt Disable flag
    return 0;           // SEI always takes the base number of cycles
}

uint8_t cpu::CLD() {
    setFlag(D, false);
    return 0;
}

uint8_t cpu::CLC() {
    setFlag(C, false);
    return 0;
}

uint8_t cpu::ASL() {
    if (lookup[opcode].addrmode == &cpu::IMM) {
        setFlag(C, (A & 0x80));
        A <<=1;
        setZN(A);
    } else {
        uint8_t temp = fetched;
        setFlag(C, (temp &0x80));
        temp <<=1;

        write(addr_abs, temp);
    }
    return 0;
}

uint8_t cpu::PHP() {
    push(P);
    return 0;
}

uint8_t cpu::BPL() {
    if (getFlag(N) == 0) {
        cycles++;
        uint16_t oldPC = PC;
        PC += addr_rel;
        if ((PC & 0xFF00) != (oldPC & 0xFF00)) {
            cycles++;
        }
    }
    return 0;
}


// ---------------- Clock / instruction flow ----------------
// Simple clock implementation that executes one instruction when cycles == 0.
// We keep addressing-mode functions as written (they read operands using PC+1/PC+2),
// so we do NOT increment PC before calling them. Instead, after op we advance PC
// according to the addressing-mode size.
void cpu::clock() {
    if (cycles == 0) {
        opcode = read(PC);
        Op ins = lookup[opcode];

        // Run addressing mode
        uint8_t add_cycles_addr = 0;
        if (ins.addrmode) add_cycles_addr = (this->*ins.addrmode)();

        // Run operation
        uint8_t add_cycles_op = 0;
        if (ins.operate) add_cycles_op = (this->*ins.operate)();

        // total cycles
        cycles = ins.cycles + add_cycles_addr + add_cycles_op;

        // Advance PC by instruction length (heuristic based on addressing-mode)
        // IMP (implied) -> 1 byte
        // IMM, ZP0, ZPX, ZPY, IZX, IZY, REL -> 2 bytes
        // ABS, ABX, ABY, IND -> 3 bytes
        auto addrmode = ins.addrmode;
        if (addrmode == &cpu::IMP) {
            PC += 1;
        } else if (addrmode == &cpu::IMM || addrmode == &cpu::ZP0 ||
                   addrmode == &cpu::ZPX || addrmode == &cpu::ZPY ||
                   addrmode == &cpu::IZX || addrmode == &cpu::IZY ||
                   addrmode == &cpu::REL) {
            PC += 2;
        } else {// ABS, ABX, ABY, IND
            PC += 3;
        }
    }

    // consume a cycle
    if (cycles > 0) cycles--;
}

// stepInstruction: execute a single full instruction (blocking until cycles consumed)
void cpu::stepInstruction() {
    // Ensure next instruction executes and run until cycles are exhausted
    cycles = 0;
    clock();
    while (cycles > 0) clock();
}

// reset: initialize registers and set PC from reset vector (0xFFFC/0xFFFD)
void cpu::reset() {
    // read reset vector
    uint16_t lo = read(0xFFFC);
    uint16_t hi = read(0xFFFD);
    PC = (hi<<8) | lo;
    SP = 0xFD;
    A = X = Y = 0;
    P = 0x24;
    cycles = 8; // warmup cycles
}

// ---------------- Lookup builder ----------------
void cpu::buildLookup() {
    // default to XXX
    for (int i = 0; i < 256; ++i) lookup[i] = {"XXX", &cpu::XXX, &cpu::IMP, 2};

    // BRK
    lookup[0x00] = {"BRK", &cpu::BRK, &cpu::IMM, 7};

    // LDA
    lookup[0xA9] = {"LDA", &cpu::LDA, &cpu::IMM, 2};
    lookup[0xA5] = {"LDA", &cpu::LDA, &cpu::ZP0, 3};
    lookup[0xB5] = {"LDA", &cpu::LDA, &cpu::ZPX, 4};
    lookup[0xAD] = {"LDA", &cpu::LDA, &cpu::ABS, 4};
    lookup[0xBD] = {"LDA", &cpu::LDA, &cpu::ABX, 4};
    lookup[0xB9] = {"LDA", &cpu::LDA, &cpu::ABY, 4};
    lookup[0xA1] = {"LDA", &cpu::LDA, &cpu::IZX, 6};
    lookup[0xB1] = {"LDA", &cpu::LDA, &cpu::IZY, 5};

    // LDX
    lookup[0xA2] = {"LDX", &cpu::LDX, &cpu::IMM, 2};
    lookup[0xA6] = {"LDX", &cpu::LDX, &cpu::ZP0, 3};
    lookup[0xB6] = {"LDX", &cpu::LDX, &cpu::ZPY, 4};
    lookup[0xAE] = {"LDX", &cpu::LDX, &cpu::ABS, 4};
    lookup[0xBE] = {"LDX", &cpu::LDX, &cpu::ABY, 4};

    // LDY
    lookup[0xA0] = {"LDY", &cpu::LDY, &cpu::IMM, 2};
    lookup[0xA4] = {"LDY", &cpu::LDY, &cpu::ZP0, 3};
    lookup[0xB4] = {"LDY", &cpu::LDY, &cpu::ZPX, 4};
    lookup[0xAC] = {"LDY", &cpu::LDY, &cpu::ABS, 4};
    lookup[0xBC] = {"LDY", &cpu::LDY, &cpu::ABX, 4};

    // STA (store accumulator)
    lookup[0x85] = {"STA", &cpu::STA, &cpu::ZP0, 3};
    lookup[0x95] = {"STA", &cpu::STA, &cpu::ZPX, 4};
    lookup[0x8D] = {"STA", &cpu::STA, &cpu::ABS, 4};
    lookup[0x9D] = {"STA", &cpu::STA, &cpu::ABX, 5};
    lookup[0x99] = {"STA", &cpu::STA, &cpu::ABY, 5};
    lookup[0x81] = {"STA", &cpu::STA, &cpu::IZX, 6};
    lookup[0x91] = {"STA", &cpu::STA, &cpu::IZY, 6};

    // TAX/TAY/TXA/TYA
    lookup[0xAA] = {"TAX", &cpu::TAX, &cpu::IMP, 2};
    lookup[0xA8] = {"TAY", &cpu::TAY, &cpu::IMP, 2};
    lookup[0x8A] = {"TXA", &cpu::TXA, &cpu::IMP, 2};
    lookup[0x98] = {"TYA", &cpu::TYA, &cpu::IMP, 2};

    lookup[0x9A] = {"TXS", &cpu::TXS, &cpu::IMP, 2};

    // INX/INY/DEX/DEY
    lookup[0xE8] = {"INX", &cpu::INX, &cpu::IMP, 2};
    lookup[0xC8] = {"INY", &cpu::INY, &cpu::IMP, 2};
    lookup[0xCA] = {"DEX", &cpu::DEX, &cpu::IMP, 2};
    lookup[0x88] = {"DEY", &cpu::DEY, &cpu::IMP, 2};

    // JMP/JSR/RTS
    lookup[0x4C] = {"JMP", &cpu::JMP, &cpu::ABS, 3};
    lookup[0x20] = {"JSR", &cpu::JSR, &cpu::ABS, 6};
    lookup[0x60] = {"RTS", &cpu::RTS, &cpu::IMP, 6};

    // flag set/clear
    lookup[0x78] = {"SEI", &cpu::SEI, &cpu::IMP, 2};
    lookup[0xD8] = {"CLD", &cpu::CLD, &cpu::IMP, 2};
    lookup[0x18] = {"CLC", &cpu::CLC, &cpu::IMP, 2};

    //ORA
    lookup[0x09] = {"ORA", &cpu::ORA, &cpu::IMM, 2};
    lookup[0x05] = {"ORA", &cpu::ORA, &cpu::ZP0, 3};
    lookup[0x15] = {"ORA", &cpu::ORA, &cpu::ZPX, 4};
    lookup[0x15] = {"ORA", &cpu::ORA, &cpu::ABS, 4};
    lookup[0x0D] = {"ORA", &cpu::ORA, &cpu::ABX, 4};
    lookup[0x1D] = {"ORA", &cpu::ORA, &cpu::ABY, 4};
    lookup[0x01] = {"ORA", &cpu::ORA, &cpu::IZX, 2};
    lookup[0x11] = {"ORA", &cpu::ORA, &cpu::IZY, 2};

    //ASL
    lookup[0x0A] = {"ASL", &cpu::ASL, &cpu::IMM, 2 };
    lookup[0x06] = {"ASL", &cpu::ASL, &cpu::ZP0, 5} ;
    lookup[0x16] = {"ASL", &cpu::ASL, &cpu::ZPX, 6 };
    lookup[0x0E] = {"ASL", &cpu::ASL, &cpu::ABS, 6};
    lookup[0x1E] = {"ASL", &cpu::ASL, &cpu::ABX, 7};

    //php
    lookup[0x08] = {"PHP", &cpu::PHP, &cpu::IMM, 3};

    //BPL
    lookup[0x10] = {"BPL", &cpu::BPL, &cpu::REL, 2};

    // NOP
    lookup[0xEA] = {"NOP", &cpu::NOP, &cpu::IMP, 2};

    // You can continue filling out the rest of the table as needed...
}

// GUI helpers
void cpu::drawFlagsGui() const {
    auto draw = [&](const char* label, bool v){ ImVec4 c = v ? ImVec4(0.2f,1.0f,0.2f,1.0f) : ImVec4(1.0f,0.2f,0.2f,1.0f); ImGui::TextColored(c, "%s", label); ImGui::SameLine(); };
    draw("C", getFlag(C)); draw("Z", getFlag(Z)); draw("I", getFlag(I)); draw("D", getFlag(D));
    draw("B", getFlag(B)); draw("U", getFlag(U)); draw("V", getFlag(V)); draw("N", getFlag(N));
    ImGui::NewLine();
}

void cpu::drawStackGui() const {
    ImGui::Text("SP: %02X", SP);
    ImGui::BeginChild("stack", ImVec2(0,200), true);
    for (int i = 0; i < 256; i += 16) {
        ImGui::Text("%04X: ", 0x0100 + i);
        ImGui::SameLine();
        for (int j = 0; j < 16; ++j) {
            uint16_t a = 0x0100 + i + j;
            uint8_t v = bus_ptr->read(a);
            if ((int)(0x0100 + SP + 1) == a) ImGui::TextColored(ImVec4(1,1,0,1), "%02X ", v);
            else ImGui::Text("%02X ", v);
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    ImGui::EndChild();
}
