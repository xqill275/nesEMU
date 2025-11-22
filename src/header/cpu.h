#ifndef CPU_H
#define CPU_H

#include <cstdint>
#include <array>
#include <string>

class bus;


struct Instruction {
    std::string name;
    uint8_t *bytes; // placeholder (not used at runtime)
    uint8_t cycles;
    // We'll store function pointers in the cpp to avoid header pollution
};

class cpu {
public:
    cpu();


    // Registers
    uint16_t PC = 0;
    uint8_t SP = 0;
    uint8_t A = 0;
    uint8_t X = 0;
    uint8_t Y = 0;
    uint8_t P = 0x24; // status


    // helper
    enum FLAGS {
        C = (1 << 0),
        Z = (1 << 1),
        I = (1 << 2),
        D = (1 << 3),
        B = (1 << 4),
        U = (1 << 5),
        V = (1 << 6),
        N = (1 << 7)
        };


    // bus
    bus* bus_ptr = nullptr;
    void connectBus(bus* bus);

    // public API
    void reset(); // reset CPU (reads vector)
    void clock(); // execute cycles (cycle-based)
    void stepInstruction(); // execute single instruction


    // read/write through bus
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);


    // GUI helpers
    void drawFlagsGui() const;
    void drawStackGui() const;

private:
    // internal
    uint8_t fetched = 0; // holds fetched data
    uint16_t addr_abs = 0; // absolute address computed by addrmode
    uint16_t addr_rel = 0; // relative address for branches
    uint8_t opcode = 0;
    uint8_t cycles = 0;


    // helpers
    inline void setFlag(FLAGS f, bool v);
    inline bool getFlag(FLAGS f) const;
    inline void setZN(uint8_t v);


    // stack helpers
    void push(uint8_t v);
    uint8_t pop();


    // addressing modes
    uint8_t IMP(); uint8_t IMM(); uint8_t ZP0(); uint8_t ZPX(); uint8_t ZPY();
    uint8_t ABS(); uint8_t ABX(); uint8_t ABY(); uint8_t IND();
    uint8_t IZX(); uint8_t IZY(); uint8_t REL();


    // operations (a subset; add more following pattern)
    uint8_t XXX();
    uint8_t BRK(); uint8_t NOP();
    uint8_t LDA(); uint8_t LDX(); uint8_t LDY();
    uint8_t STA();
    uint8_t TAX(); uint8_t TAY(); uint8_t TXA(); uint8_t TYA();
    uint8_t INX(); uint8_t INY(); uint8_t DEX(); uint8_t DEY();
    uint8_t JMP(); uint8_t JSR(); uint8_t RTS();


    // lookup table
    struct Op {
        const char* name;
        uint8_t (cpu::*operate)();
        uint8_t (cpu::*addrmode)();
        uint8_t cycles;
    };
    std::array<Op, 256> lookup;


    void buildLookup();
};


#endif // CPU_H