// src/cpu.cpp
#include "header/cpu.h"
#include "header/Bus.h"
#include <iostream>
#include "external/imgui/imgui.h"

cpu::cpu() {
    buildLookup();
}

void cpu::connectBus(bus* b) { bus_ptr = b; }

uint8_t cpu::read(uint16_t addr) { return bus_ptr ? bus_ptr->read(addr) : 0x00; }
void cpu::write(uint16_t addr, uint8_t data) { if (bus_ptr) bus_ptr->write(addr, data); }

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
inline void cpu::setFlag(FLAGS f, bool v) { if (v) P |= f; else P &= ~f; }
inline bool cpu::getFlag(FLAGS f) const { return (P & f) != 0; }
inline void cpu::setZN(uint8_t v) { setFlag(Z, v == 0); setFlag(N, (v & 0x80) != 0); }

// -----------------------------------------------------------------------------
// Stack
// -----------------------------------------------------------------------------
void cpu::push(uint8_t v) {
    write(0x0100 + SP, v);
    SP--;
}

uint8_t cpu::pop() {
    SP++;
    return read(0x0100 + SP);
}

// -----------------------------------------------------------------------------
// Fetch operand (based on addrmode result)
// -----------------------------------------------------------------------------
uint8_t cpu::fetch() {
    // If addressing mode is implied, fetched is already set to A by IMP
    if (lookup[opcode].addrmode == &cpu::IMP) {
        return fetched;
    } else {
        fetched = read(addr_abs);
        return fetched;
    }
}

// -----------------------------------------------------------------------------
// Addressing modes
// Each mode reads operand(s) from memory at PC+1/2... and then advances PC to
// point to the next instruction (so operations can overwrite PC when needed).
// -----------------------------------------------------------------------------

// Implied / Accumulator: some instructions use the accumulator (A) directly,
// but we don't declare a separate ACC() in the header — IMP is used for both
// implied and accumulator opcodes. IMP sets fetched = A and advances PC by 1.
uint8_t cpu::IMP() {
    fetched = A;
    PC += 1;
    return 0;
}

// Immediate: operand is next byte
uint8_t cpu::IMM() {
    addr_abs = PC + 1;
    PC += 2;
    return 0;
}

// Zero Page
uint8_t cpu::ZP0() {
    addr_abs = read(PC + 1) & 0x00FF;
    PC += 2;
    return 0;
}

// Zero Page,X
uint8_t cpu::ZPX() {
    addr_abs = (uint8_t)(read(PC + 1) + X) & 0x00FF;
    PC += 2;
    return 0;
}

// Zero Page,Y
uint8_t cpu::ZPY() {
    addr_abs = (uint8_t)(read(PC + 1) + Y) & 0x00FF;
    PC += 2;
    return 0;
}

// Absolute
uint8_t cpu::ABS() {
    uint16_t lo = read(PC + 1);
    uint16_t hi = read(PC + 2);
    addr_abs = (hi << 8) | lo;
    PC += 3;
    return 0;
}

// Absolute,X (returns 1 if page crossed)
uint8_t cpu::ABX() {
    uint16_t lo = read(PC + 1);
    uint16_t hi = read(PC + 2);
    uint16_t base = (hi << 8) | lo;
    addr_abs = base + X;
    PC += 3;
    return ((base & 0xFF00) != (addr_abs & 0xFF00)) ? 1 : 0;
}

// Absolute,Y (returns 1 if page crossed)
uint8_t cpu::ABY() {
    uint16_t lo = read(PC + 1);
    uint16_t hi = read(PC + 2);
    uint16_t base = (hi << 8) | lo;
    addr_abs = base + Y;
    PC += 3;
    return ((base & 0xFF00) != (addr_abs & 0xFF00)) ? 1 : 0;
}

// Indirect (used by JMP indirect) — emulate 6502 page-boundary bug
uint8_t cpu::IND() {
    uint16_t ptr_lo = read(PC + 1);
    uint16_t ptr_hi = read(PC + 2);
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;

    // Emulate page-boundary hardware bug:
    // if low byte is $xxFF, the high byte wraps on the same page.
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));

    addr_abs = (hi << 8) | lo;
    PC += 3;
    return 0;
}

// (Indirect,X)
uint8_t cpu::IZX() {
    uint8_t zp = read(PC + 1);
    uint8_t t = (uint8_t)(zp + X);
    uint8_t lo = read(t & 0x00FF);
    uint8_t hi = read((t + 1) & 0x00FF);
    addr_abs = (hi << 8) | lo;
    PC += 2;
    return 0;
}

// (Indirect),Y
uint8_t cpu::IZY() {
    uint8_t zp = read(PC + 1);
    uint8_t lo = read(zp & 0x00FF);
    uint8_t hi = read((zp + 1) & 0x00FF);
    uint16_t base = (hi << 8) | lo;
    addr_abs = base + Y;
    PC += 2;
    return ((base & 0xFF00) != (addr_abs & 0xFF00)) ? 1 : 0;
}

// Relative (for branches) — store signed offset in addr_rel and advance PC
uint8_t cpu::REL() {
    addr_rel = static_cast<int8_t>(read(PC + 1));
    PC += 2;
    return 0;
}

// -----------------------------------------------------------------------------
// Operations
// -----------------------------------------------------------------------------

uint8_t cpu::XXX() {
    std::cout << "Unknown opcode: $"
              << std::hex << (int)opcode
              << " at PC $"
              << std::hex << PC << "\n";
    return 0;
}

uint8_t cpu::NOP() {
    return 0;
}

// BRK: using IMM as addressing mode in lookup (so PC already points to next instr)
// BRK pushes PC and P, sets I, loads vector from 0xFFFE/0xFFFF.
uint8_t cpu::BRK() {
    // At this point addrmode IMM has already advanced PC past the BRK padding
    // Push program counter (high then low)
    uint16_t return_addr = PC+1;
    push((return_addr >> 8) & 0xFF);
    push(return_addr & 0xFF);

    // Set B and U in pushed status
    setFlag(B, true);
    setFlag(U, true);

    // Push status
    push(P);

    // Set interrupt disable
    setFlag(I, true);

    // Load IRQ vector
    uint16_t lo = read(0xFFFE);
    uint16_t hi = read(0xFFFF);
    PC = (hi << 8) | lo;

    return 0;
}

uint8_t cpu::ORA() {
    // fetch operand then OR with A
    fetch();
    A = A | fetched;
    setZN(A);
    return 0;
}

uint8_t cpu::LDA() {
    fetch();
    A = fetched;
    setZN(A);
    return 0;
}

uint8_t cpu::LDX() {
    fetch();
    X = fetched;
    setZN(X);
    return 0;
}

uint8_t cpu::LDY() {
    fetch();
    Y = fetched;
    setZN(Y);
    return 0;
}

uint8_t cpu::STA() {
    // store A into addr_abs
    std::cout << "Writing value: " << std::hex << (int)A << " To:" << addr_abs << std::endl;
    write(addr_abs, A);
    return 0;
}

uint8_t cpu::TAX() { X = A; setZN(X); return 0; }
uint8_t cpu::TAY() { Y = A; setZN(Y); return 0; }
uint8_t cpu::TXA() { A = X; setZN(A); return 0; }
uint8_t cpu::TYA() { A = Y; setZN(A); return 0; }

uint8_t cpu::TXS() { SP = X; return 0; }

uint8_t cpu::INX() { X++; setZN(X); return 0; }
uint8_t cpu::INY() { Y++; setZN(Y); return 0; }
uint8_t cpu::DEX() { X--; setZN(X); return 0; }
uint8_t cpu::DEY() { Y--; setZN(Y); return 0; }

// JMP absolute: addressing mode IND or ABS already advanced PC to next instr.
// JMP must set PC to target addr_abs.
uint8_t cpu::JMP() {
    PC = addr_abs;
    return 0;
}

// JSR: addressing mode ABS advanced PC to next instruction already.
// Push return address (PC - 1) per 6502 semantics, then set PC to target.
uint8_t cpu::JSR() {
    // Return address is PC - 1 (because PC already points to next instruction)
    uint16_t return_addr = PC - 1;
    push((return_addr >> 8) & 0xFF);
    push(return_addr & 0xFF);

    // Jump to target address (addr_abs)
    PC = addr_abs;
    return 0;
}

// RTS: pull return address and set PC = return + 1
uint8_t cpu::RTS() {
    uint16_t lo = pop();
    uint16_t hi = pop();
    PC = ((hi << 8) | lo) + 1;
    return 0;
}

// flags
uint8_t cpu::SEI() { setFlag(I, true); return 0; }
uint8_t cpu::CLD() { setFlag(D, false); return 0; }
uint8_t cpu::CLC() { setFlag(C, false); return 0; }

// ASL: if addressing mode is IMP (accumulator), operate on A; otherwise on memory.
uint8_t cpu::ASL() {
    if (lookup[opcode].addrmode == &cpu::IMP) {
        // Accumulator mode
        setFlag(C, (A & 0x80) != 0);
        A <<= 1;
        setZN(A);
    } else {
        fetch(); // fetched contains memory value at addr_abs
        uint8_t temp = fetched;
        setFlag(C, (temp & 0x80) != 0);
        temp <<= 1;
        write(addr_abs, temp);
        setZN(temp);
    }
    return 0;
}

// PHP must push P with B and U bits set
uint8_t cpu::PHP() {
    uint8_t flags = P | B | U;
    push(flags);
    return 0;
}

// BPL implementation: PC already points to next instruction (after REL).
// Branch target = PC + addr_rel
uint8_t cpu::BPL() {
    if (!getFlag(N)) {
        cycles++;
        uint16_t oldPC = PC;
        PC = PC + addr_rel;
        if ((PC & 0xFF00) != (oldPC & 0xFF00)) cycles++;
    }
    return 0;
}

uint8_t cpu::AND()
{
    // Fetch the operand (based on the current addressing mode)
    fetch();

    // Perform A = A & fetched
    A = A & fetched;

    // Set Z and N flags
    setZN(A);

    return 1; // This instruction adds 1 cycle
}

uint8_t cpu::BEQ()
{
    if (getFlag(Z))
    {
        cycles++; // Branch taken costs 1 extra cycle

        uint16_t oldPC = PC;
        PC += addr_rel;

        // Add another cycle if crossing a page boundary
        if ((PC & 0xFF00) != (oldPC & 0xFF00))
            cycles++;
    }

    return 0;
}

uint8_t cpu::BIT() {
    fetch();  // fetches the value at addr_abs into `fetched`

    uint8_t result = A & fetched;

    // Zero flag = set if (A & M) == 0
    setFlag(Z, result == 0x00);

    // Bit 6 -> Overflow flag
    setFlag(V, fetched & 0x40);

    // Bit 7 -> Negative flag
    setFlag(N, fetched & 0x80);

    return 0;
}

uint8_t cpu::ROL() {
    fetch();  // get value (either A or memory)

    uint8_t oldCarry = getFlag(C) ? 1 : 0;
    uint8_t result = (fetched << 1) | oldCarry;

    setFlag(C, fetched & 0x80);   // bit 7 goes into carry
    setZN(result);

    // If addressing mode was implied (i.e., accumulator)
    if (lookup[opcode].addrmode == &cpu::IMP) {
        A = result;
    } else {
        write(addr_abs, result);
    }

    return 0;
}



// -----------------------------------------------------------------------------
// Clock / instruction flow
// -----------------------------------------------------------------------------
void cpu::clock() {
    if (cycles == 0) {
        // Fetch opcode at current PC
        opcode = read(PC);
        const Op& ins = lookup[opcode];
        std::cout << ins.name << " - " << std::hex << (int)opcode << std::endl;
        // Run addressing mode (it will advance PC to next instruction by design)
        uint8_t add_cycles_addr = 0;
        if (ins.addrmode) add_cycles_addr = (this->*ins.addrmode)();

        // Run operation which may modify PC (e.g. jumps)
        uint8_t add_cycles_op = 0;
        if (ins.operate) add_cycles_op = (this->*ins.operate)();

        // Total cycles
        cycles = ins.cycles + add_cycles_addr + add_cycles_op;
    }

    // consume a cycle
    if (cycles > 0) cycles--;
}

// stepInstruction: execute a single full instruction (blocking until cycles consumed)
void cpu::stepInstruction() {
    cycles = 0;
    clock();
    while (cycles > 0) clock();
}

// reset: initialize registers and set PC from reset vector (0xFFFC/0xFFFD)
void cpu::reset() {
    uint16_t lo = read(0xFFFC);
    uint16_t hi = read(0xFFFD);
    PC = (hi << 8) | lo;

    SP = 0xFD;
    A = X = Y = 0x00;
    P = 0x24;

    cycles = 8; // warmup cycles
}

void cpu::nmi() {
    // Push PC to stack (high byte first)
    push((PC >> 8) & 0x00FF);
    push(PC & 0x00FF);

    // Push status register
    setFlag(B, false);
    setFlag(U, true);
    setFlag(I, true);
    push(P);

    // Read NMI vector ($FFFA-$FFFB)
    uint16_t lo = read(0xFFFA);
    uint16_t hi = read(0xFFFB);
    PC = (hi << 8) | lo;

    // NMI takes 8 cycles
    cycles = 8;
}

uint8_t cpu::PLP() {
    P = pop();
    P &= ~FLAGS::B;   // clear B flag
    P |= FLAGS::U;    // set unused bit
    return 0;
}

uint8_t cpu::SEC() {
    setFlag(C, true);
    return 0;
}

uint8_t cpu::RTI() {
    // Pull status register (but ensure unused flag stays set)
    P = pop();
    P &= ~B;     // Clear Break flag (hardware behavior)
    P |= U;      // Unused flag always set

    // Pull PC low byte, then high byte
    uint8_t lo = pop();
    uint8_t hi = pop();
    PC = (hi << 8) | lo;

    return 0;
}

uint8_t cpu::EOR() {
    fetch();
    A = A ^ fetched;
    setZN(A);
    return 0;
}

uint8_t cpu::LSR() {
    fetch();

    // Carry = bit 0
    setFlag(C, fetched & 0x01);

    uint8_t result = fetched >> 1;

    if (lookup[opcode].addrmode == &cpu::IMP) {
        A = result;
        setZN(A);
    } else {
        write(addr_abs, result);
        setZN(result);
    }

    return 0;
}

uint8_t cpu::PHA() {
    push(A);
    return 0;

}

uint8_t cpu::BVC() {
    if (getFlag(V) == 0) {
        cycles++;  // branching adds 1 cycle

        uint16_t prevPC = PC;
        PC += addr_rel;

        // Page crossed?
        if ((PC & 0xFF00) != (prevPC & 0xFF00)) {
            cycles++;  // extra cycle on page cross
        }
    }
    return 0;
}

uint8_t cpu::CLI() {
    setFlag(I, false);
    return 0;
}

uint8_t cpu::ADC() {
    fetch();

    uint16_t sum = (uint16_t)A + (uint16_t)fetched + (uint16_t)getFlag(C);

    // Carry (unsigned overflow)
    setFlag(C, sum > 0xFF);

    // Zero
    setFlag(Z, (sum & 0xFF) == 0);

    // Overflow (signed overflow detection)
    bool overflow = (~(A ^ fetched) & (A ^ sum) & 0x80);
    setFlag(V, overflow);

    // Negative
    setFlag(N, sum & 0x80);

    A = sum & 0xFF;

    return 1;   // ADD EXTRA CYCLE if page crossed (ADC uses returned value)
}

uint8_t cpu::ROR() {
    fetch();

    uint8_t old_carry = getFlag(C);
    uint8_t new_carry = fetched & 0x01;

    uint8_t result = (fetched >> 1) | (old_carry << 7);

    setFlag(C, new_carry);
    setFlag(Z, result == 0);
    setFlag(N, result & 0x80);

    if (lookup[opcode].addrmode == &cpu::IMP)
        A = result;
    else
        write(addr_abs, result);

    return 0;
}

uint8_t cpu::PLA() {
    A = pop();
    setZN(A);
    return 0;
}

uint8_t cpu::BVS() {
    if (getFlag(V)) {
        cycles++;
        uint16_t newPC = PC + addr_rel;

        if ((newPC & 0xFF00) != (PC & 0xFF00)) {
            cycles++; // page cross
        }

        PC = newPC;
    }
    return 0;
}

uint8_t cpu::STY() {
    write(addr_abs, Y);
    return 0;
}

uint8_t cpu::STX() {
    write(addr_abs, X);
    return 0;
}

uint8_t cpu::BCS() {
    if (getFlag(C)) {
        cycles++;
        uint16_t newPC = PC + addr_rel;

        if ((newPC & 0xFF00) != (PC & 0xFF00))
            cycles++;

        PC = newPC;
    }
    return 0;
}

uint8_t cpu::CLV() {
    setFlag(V, false);
    return 0;
}

uint8_t cpu::BCC() {
    if (getFlag(C) == 0) {        // Branch if Carry Clear
        cycles++;                // Branch successful → add 1 cycle

        // If signed offset is negative, sign-extend it
        PC += addr_rel;

        // If branch crosses a page, add another cycle
        if ((PC & 0xFF00) != ((PC - addr_rel) & 0xFF00)) {
            cycles++;
        }
    }
    return 0;
}

uint8_t cpu::CPY() {
    fetch();
    uint16_t temp = (uint16_t)Y - (uint16_t)fetched;

    setFlag(C, Y >= fetched);              // Carry = 1 if Y >= M
    setFlag(Z, (temp & 0x00FF) == 0x0000); // Zero = 1 if equal
    setFlag(N, temp & 0x0080);             // Negative = bit 7

    return 0;
}

uint8_t cpu::CMP() {
    fetch();
    uint16_t temp = (uint16_t)A - (uint16_t)fetched;

    setFlag(C, A >= fetched);              // Carry = A >= M
    setFlag(Z, (temp & 0x00FF) == 0x0000); // Zero = A == M
    setFlag(N, temp & 0x0080);             // Negative = bit 7

    return 0;
}

uint8_t cpu::DEC() {
    fetch();
    uint8_t temp = fetched - 1;

    write(addr_abs, temp);

    setFlag(Z, temp == 0x00);
    setFlag(N, temp & 0x80);

    return 0;
}

uint8_t cpu::BNE() {
    if (getFlag(Z) == 0) {
        cycles++;   // Branch successful → 1 extra cycle
        uint16_t prevPC = PC;
        PC += addr_rel;

        // Page boundary crossed?
        if ((PC & 0xFF00) != (prevPC & 0xFF00))
            cycles++;
    }
    return 0;
}

uint8_t cpu::CPX() {
    fetch();
    uint16_t temp = (uint16_t)X - (uint16_t)fetched;

    setFlag(C, X >= fetched);
    setFlag(Z, (temp & 0x00FF) == 0);
    setFlag(N, temp & 0x0080);

    return 0;
}

uint8_t cpu::SBC() {
    fetch();

    uint16_t value = ((uint16_t)fetched) ^ 0x00FF;
    uint16_t temp = (uint16_t)A + value + (uint16_t)getFlag(C);

    setFlag(C, temp & 0xFF00);                     // Carry = NOT borrow
    setFlag(Z, (temp & 0x00FF) == 0);
    setFlag(V, (temp ^ (uint16_t)A) & (temp ^ value) & 0x0080);
    setFlag(N, temp & 0x0080);

    A = temp & 0x00FF;
    return 0;
}

uint8_t cpu::INC() {
    fetch();
    uint8_t temp = fetched + 1;
    write(addr_abs, temp);

    setZN(temp);
    return 0;
}

uint8_t cpu::SED() {
    // Set Decimal Mode flag — NES ignores it, but it still gets set.
    setFlag(D, true);
    return 0;
}

bool cpu::complete() {
    return cycles == 0;
}


// -----------------------------------------------------------------------------
// Lookup builder
// -----------------------------------------------------------------------------
void cpu::buildLookup() {
    // Default to XXX
    for (int i = 0; i < 256; ++i) lookup[i] = {"XXX", &cpu::XXX, &cpu::IMP, 2};

    // BRK: use IMM so we consume padding byte
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
    lookup[0x6C] = {"JMP", &cpu::JMP, &cpu::IND, 5}; // indirect JMP
    lookup[0x20] = {"JSR", &cpu::JSR, &cpu::ABS, 6};
    lookup[0x60] = {"RTS", &cpu::RTS, &cpu::IMP, 6};

    // flag set/clear
    lookup[0x78] = {"SEI", &cpu::SEI, &cpu::IMP, 2};
    lookup[0xD8] = {"CLD", &cpu::CLD, &cpu::IMP, 2};
    lookup[0x18] = {"CLC", &cpu::CLC, &cpu::IMP, 2};

    // ORA (corrected)
    lookup[0x09] = {"ORA", &cpu::ORA, &cpu::IMM, 2};
    lookup[0x05] = {"ORA", &cpu::ORA, &cpu::ZP0, 3};
    lookup[0x15] = {"ORA", &cpu::ORA, &cpu::ZPX, 4};
    lookup[0x0D] = {"ORA", &cpu::ORA, &cpu::ABS, 4};
    lookup[0x1D] = {"ORA", &cpu::ORA, &cpu::ABX, 4};
    lookup[0x19] = {"ORA", &cpu::ORA, &cpu::ABY, 4};
    lookup[0x01] = {"ORA", &cpu::ORA, &cpu::IZX, 6};
    lookup[0x11] = {"ORA", &cpu::ORA, &cpu::IZY, 5};

    // ASL
    lookup[0x0A] = {"ASL", &cpu::ASL, &cpu::IMP, 2}; // accumulator (IMP used)
    lookup[0x06] = {"ASL", &cpu::ASL, &cpu::ZP0, 5};
    lookup[0x16] = {"ASL", &cpu::ASL, &cpu::ZPX, 6};
    lookup[0x0E] = {"ASL", &cpu::ASL, &cpu::ABS, 6};
    lookup[0x1E] = {"ASL", &cpu::ASL, &cpu::ABX, 7};

    // PHP
    lookup[0x08] = {"PHP", &cpu::PHP, &cpu::IMP, 3};

    // BPL
    lookup[0x10] = {"BPL", &cpu::BPL, &cpu::REL, 2};

    // AND
    lookup[0x29] = {"AND", &cpu::AND, &cpu::IMM, 2};
    lookup[0x25] = {"AND", &cpu::AND, &cpu::ZP0, 3};
    lookup[0x35] = {"AND", &cpu::AND, &cpu::ZPX, 4};
    lookup[0x2D] = {"AND", &cpu::AND, &cpu::ABS, 4};
    lookup[0x3D] = {"AND", &cpu::AND, &cpu::ABX, 4};
    lookup[0x39] = {"AND", &cpu::AND, &cpu::ABY, 4};
    lookup[0x21] = {"AND", &cpu::AND, &cpu::IZX, 6};
    lookup[0x31] = {"AND", &cpu::AND, &cpu::IZY, 5};

    //BEQ
    lookup[0xF0] = { "BEQ", &cpu::BEQ, &cpu::REL, 2 };

    //BIT
    lookup[0x24] = { "BIT", &cpu::BIT, &cpu::ZP0, 3 };
    lookup[0x2C] = { "BIT", &cpu::BIT, &cpu::ABS, 4 };

    //ROL
    lookup[0x2A] = { "ROL", &cpu::ROL, &cpu::IMP, 2 }; // Accumulator
    lookup[0x26] = { "ROL", &cpu::ROL, &cpu::ZP0, 5 };
    lookup[0x36] = { "ROL", &cpu::ROL, &cpu::ZPX, 6 };
    lookup[0x2E] = { "ROL", &cpu::ROL, &cpu::ABS, 6 };
    lookup[0x3E] = { "ROL", &cpu::ROL, &cpu::ABX, 7 };

    //PLP
    lookup[0x28] = { "PLP", &cpu::PLP, &cpu::IMP, 4 };

    //SEC
    lookup[0x38] = { "SEC", &cpu::SEC, &cpu::IMP, 2 };

    //EOR
    lookup[0x49] = { "EOR", &cpu::EOR, &cpu::IMM, 2 };
    lookup[0x45] = { "EOR", &cpu::EOR, &cpu::ZP0, 3 };
    lookup[0x55] = { "EOR", &cpu::EOR, &cpu::ZPX, 4 };
    lookup[0x4D] = { "EOR", &cpu::EOR, &cpu::ABS, 4 };
    lookup[0x5D] = { "EOR", &cpu::EOR, &cpu::ABX, 4 }; // +1 if page crossed
    lookup[0x59] = { "EOR", &cpu::EOR, &cpu::ABY, 4 }; // +1 if page crossed
    lookup[0x41] = { "EOR", &cpu::EOR, &cpu::IZX, 6 };
    lookup[0x51] = { "EOR", &cpu::EOR, &cpu::IZY, 5 }; // +1 if page crossed

    //LSR
    lookup[0x4A] = { "LSR", &cpu::LSR, &cpu::IMP, 2 };
    lookup[0x46] = { "LSR", &cpu::LSR, &cpu::ZP0, 5 };
    lookup[0x56] = { "LSR", &cpu::LSR, &cpu::ZPX, 6 };
    lookup[0x4E] = { "LSR", &cpu::LSR, &cpu::ABS, 6 };
    lookup[0x5E] = { "LSR", &cpu::LSR, &cpu::ABX, 7 };

    //PHA
    lookup[0x48] = { "PHA", &cpu::PHA, &cpu::IMP, 3 };

    //RTI
    lookup[0x40] = { "RTI", &cpu::RTI, &cpu::IMP, 6 };

    //BVC
    lookup[0x50] = { "BVC", &cpu::BVC, &cpu::REL, 2 };

    //CLI
    lookup[0x58] = { "CLI", &cpu::CLI, &cpu::IMP, 2 };

    //ADC
    lookup[0x69] = {"ADC", &cpu::ADC, &cpu::IMM, 2};
    lookup[0x65] = {"ADC", &cpu::ADC, &cpu::ZP0, 3};
    lookup[0x75] = {"ADC", &cpu::ADC, &cpu::ZPX, 4};
    lookup[0x6D] = {"ADC", &cpu::ADC, &cpu::ABS, 4};
    lookup[0x7D] = {"ADC", &cpu::ADC, &cpu::ABX, 4};
    lookup[0x79] = {"ADC", &cpu::ADC, &cpu::ABY, 4};
    lookup[0x61] = {"ADC", &cpu::ADC, &cpu::IZX, 6};
    lookup[0x71] = {"ADC", &cpu::ADC, &cpu::IZY, 5};

    lookup[0x6A] = {"ROR", &cpu::ROR, &cpu::IMP, 2};
    lookup[0x66] = {"ROR", &cpu::ROR, &cpu::ZP0, 5};
    lookup[0x76] = {"ROR", &cpu::ROR, &cpu::ZPX, 6};
    lookup[0x6E] = {"ROR", &cpu::ROR, &cpu::ABS, 6};
    lookup[0x7E] = {"ROR", &cpu::ROR, &cpu::ABX, 7};

    lookup[0x68] = {"PLA", &cpu::PLA, &cpu::IMP, 4};

    lookup[0x70] = { "BVS", &cpu::BVS, &cpu::REL, 2 };

    lookup[0x84] = { "STY", &cpu::STY, &cpu::ZP0, 3 };
    lookup[0x94] = { "STY", &cpu::STY, &cpu::ZPX, 4 };
    lookup[0x8C] = { "STY", &cpu::STY, &cpu::ABS, 4 };

    lookup[0x86] = { "STX", &cpu::STX, &cpu::ZP0, 3 };
    lookup[0x96] = { "STX", &cpu::STX, &cpu::ZPY, 4 };
    lookup[0x8E] = { "STX", &cpu::STX, &cpu::ABS, 4 };

    lookup[0xB0] = { "BCS", &cpu::BCS, &cpu::REL, 2 };

    lookup[0xB8] = { "CLV", &cpu::CLV, &cpu::IMP, 2 };

    lookup[0x90] = { "BCC", &cpu::BCC, &cpu::REL, 2 };

    lookup[0xC0] = { "CPY", &cpu::CPY, &cpu::IMM, 2 };
    lookup[0xC4] = { "CPY", &cpu::CPY, &cpu::ZP0, 3 };
    lookup[0xCC] = { "CPY", &cpu::CPY, &cpu::ABS, 4 };

    lookup[0xC9] = { "CMP", &cpu::CMP, &cpu::IMM, 2 };
    lookup[0xC5] = { "CMP", &cpu::CMP, &cpu::ZP0, 3 };
    lookup[0xD5] = { "CMP", &cpu::CMP, &cpu::ZPX, 4 };
    lookup[0xCD] = { "CMP", &cpu::CMP, &cpu::ABS, 4 };
    lookup[0xDD] = { "CMP", &cpu::CMP, &cpu::ABX, 4 };
    lookup[0xD9] = { "CMP", &cpu::CMP, &cpu::ABY, 4 };
    lookup[0xC1] = { "CMP", &cpu::CMP, &cpu::IZX, 6 };
    lookup[0xD1] = { "CMP", &cpu::CMP, &cpu::IZY, 5 };

    lookup[0xC6] = { "DEC", &cpu::DEC, &cpu::ZP0, 5 };
    lookup[0xD6] = { "DEC", &cpu::DEC, &cpu::ZPX, 6 };
    lookup[0xCE] = { "DEC", &cpu::DEC, &cpu::ABS, 6 };
    lookup[0xDE] = { "DEC", &cpu::DEC, &cpu::ABX, 7 };

    lookup[0xD0] = { "BNE", &cpu::BNE, &cpu::REL, 2 };

    lookup[0xE0] = { "CPX", &cpu::CPX, &cpu::IMM, 2 };
    lookup[0xE4] = { "CPX", &cpu::CPX, &cpu::ZP0, 3 };
    lookup[0xEC] = { "CPX", &cpu::CPX, &cpu::ABS, 4 };


    lookup[0xE9] = { "SBC", &cpu::SBC, &cpu::IMM, 2 };
    lookup[0xE5] = { "SBC", &cpu::SBC, &cpu::ZP0, 3 };
    lookup[0xF5] = { "SBC", &cpu::SBC, &cpu::ZPX, 4 };
    lookup[0xED] = { "SBC", &cpu::SBC, &cpu::ABS, 4 };
    lookup[0xFD] = { "SBC", &cpu::SBC, &cpu::ABX, 4 }; // +1 on page cross
    lookup[0xF9] = { "SBC", &cpu::SBC, &cpu::ABY, 4 }; // +1 on page cross
    lookup[0xE1] = { "SBC", &cpu::SBC, &cpu::IZX, 6 };
    lookup[0xF1] = { "SBC", &cpu::SBC, &cpu::IZY, 5 }; // +1 on page cross


    lookup[0xE6] = { "INC", &cpu::INC, &cpu::ZP0, 5 };
    lookup[0xF6] = { "INC", &cpu::INC, &cpu::ZPX, 6 };
    lookup[0xEE] = { "INC", &cpu::INC, &cpu::ABS, 6 };
    lookup[0xFE] = { "INC", &cpu::INC, &cpu::ABX, 7 };

    lookup[0xF8] = { "SED", &cpu::SED, &cpu::IMP, 2 };

    // NOP
    lookup[0xEA] = {"NOP", &cpu::NOP, &cpu::IMP, 2};

    // You can continue filling out the rest of the table as needed...
}

// -----------------------------------------------------------------------------
// GUI helpers (unchanged logic, minor cleanup)
// -----------------------------------------------------------------------------
void cpu::drawFlagsGui() const {
    auto draw = [&](const char* label, bool v) {
        ImVec4 c = v ? ImVec4(0.2f,1.0f,0.2f,1.0f) : ImVec4(1.0f,0.2f,0.2f,1.0f);
        ImGui::TextColored(c, "%s", label);
        ImGui::SameLine();
    };

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
            uint8_t v = bus_ptr ? bus_ptr->read(a) : 0x00;
            if ((int)(0x0100 + SP + 1) == a) ImGui::TextColored(ImVec4(1,1,0,1), "%02X ", v);
            else ImGui::Text("%02X ", v);
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    ImGui::EndChild();
}
