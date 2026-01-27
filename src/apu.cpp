// src/apu.cpp
#include "header/apu.h"

// Length counter lookup table (32 entries)
uint8_t apu::lengthTable(uint8_t idx) {
    static constexpr uint8_t table[32] = {
        10,254, 20,  2, 40,  4, 80,  6,
        160, 8, 60, 10, 14, 12, 26, 14,
        12, 16, 24, 18, 48, 20, 96, 22,
        192,24, 72, 26, 16, 28, 32, 30
    };
    return table[idx & 0x1F];
}

void apu::reset() {
    for (auto& b : reg) b = 0x00;

    frame_mode = 0;
    irq_inhibit = false;
    frame_irq = false;

    cpu_cycle = 0;

    p1 = {};
    p2 = {};

    m_audioWrite.store(0, std::memory_order_relaxed);
    m_audioRead.store(0, std::memory_order_relaxed);
    m_samplePhase = 0.0;
}

uint8_t apu::debugReg(uint16_t addr) const {
    if (addr < 0x4000 || addr > 0x4017) return 0x00;
    return reg[addr - 0x4000];
}

uint8_t apu::debugStatus4015() const {
    uint8_t s = 0;
    if (p1.length_counter > 0) s |= (1 << 0);
    if (p2.length_counter > 0) s |= (1 << 1);
    // pulse2/tri/noise/dmc later
    if (frame_irq) s |= (1 << 6);
    return s;
}

uint8_t apu::cpuRead(uint16_t addr, bool readonly) {
    (void)readonly;

    if (addr == 0x4015) {
        uint8_t s = 0;
        if (p1.length_counter > 0) s |= (1 << 0);
        if (p2.length_counter > 0) s |= (1 << 1);
        if (frame_irq) s |= (1 << 6);

        // Reading $4015 clears frame IRQ
        frame_irq = false;
        return s;
    }

    return 0x00;
}

void apu::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr < 0x4000 || addr > 0x4017) return;

    R(addr) = data;

    // -------- Pulse 1 registers ($4000-$4003) --------
    if (addr == 0x4000) {
        p1.duty = (data >> 6) & 0x03;
        p1.length_halt = (data & 0x20) != 0;
        p1.constant_volume = (data & 0x10) != 0;
        p1.volume = data & 0x0F;
        p1.env_start = true;
        return;
    }

    if (addr == 0x4001) {
        // Sweep not implemented yet (store mirror only)
        return;
    }

    if (addr == 0x4002) {
        // Timer low 8 bits
        p1.timer = (p1.timer & 0xFF00) | data;
        return;
    }

    if (addr == 0x4003) {
        // Timer high 3 bits + length counter load + sequencer reset
        p1.timer = (p1.timer & 0x00FF) | ((uint16_t)(data & 0x07) << 8);

        // Load length counter if enabled
        uint8_t len_idx = (data >> 3) & 0x1F;
        if (p1.enabled) {
            p1.length_counter = lengthTable(len_idx);
        }

        // Restart envelope & reset sequencer phase
        p1.env_start = true;
        p1.seq_step = 0;

        return;
    }

    // -------- Channel enables ($4015) --------
    if (addr == 0x4015) {
        p1.enabled = (data & 0x01) != 0;
        p2.enabled = (data & 0x02) != 0;

        if (!p1.enabled) p1.length_counter = 0;
        if (!p2.enabled) p2.length_counter = 0;

        return;
    }

    // -------- Frame counter ($4017) --------
    if (addr == 0x4017) {
        frame_mode = (data & 0x80) ? 1 : 0;
        irq_inhibit = (data & 0x40) != 0;

        if (irq_inhibit) frame_irq = false;

        // Real hardware clocks immediately in 5-step mode on write (often emulated);
        // we’ll ignore for now to keep things simple.
        return;
    }
    // -------- Pulse 2 registers ($4004-$4007) --------
    if (addr == 0x4004) {
        p2.duty = (data >> 6) & 0x03;
        p2.length_halt = (data & 0x20) != 0;
        p2.constant_volume = (data & 0x10) != 0;
        p2.volume = data & 0x0F;
        p2.env_start = true;
        return;
    }

    if (addr == 0x4005) {
        // Sweep not implemented yet (store mirror only)
        return;
    }

    if (addr == 0x4006) {
        p2.timer = (p2.timer & 0xFF00) | data;
        return;
    }

    if (addr == 0x4007) {
        p2.timer = (p2.timer & 0x00FF) | ((uint16_t)(data & 0x07) << 8);

        uint8_t len_idx = (data >> 3) & 0x1F;
        if (p2.enabled) {
            p2.length_counter = lengthTable(len_idx);
        }

        p2.env_start = true;
        p2.seq_step = 0;
        return;
    }
    // $4004+ later for other channels
}

void apu::clockEnvelope(Pulse& p) {
    // Envelope unit (quarter-frame)
    if (p.env_start) {
        p.env_start = false;
        p.env_decay = 15;
        p.env_divider = p.volume;
    } else {
        if (p.env_divider == 0) {
            p.env_divider = p.volume;

            if (p.env_decay == 0) {
                if (p.length_halt) {
                    p.env_decay = 15; // loop
                }
            } else {
                p.env_decay--;
            }
        } else {
            p.env_divider--;
        }
    }
}

void apu::clockLengthCounter(Pulse& p) {
    // Half-frame: length counters tick if not halted
    if (!p.length_halt && p.length_counter > 0) {
        p.length_counter--;
    }
}

void apu::quarterFrame() {
    clockEnvelope(p1);
    clockEnvelope(p2);
}

void apu::halfFrame() {
    clockLengthCounter(p1);
    clockLengthCounter(p2);
    // sweep later
}

void apu::clockFrameSequencer() {
    // Frame sequencer runs at 240Hz-ish; easiest is to schedule by CPU cycles.
    //
    // NTSC frame counter steps are commonly emulated using these CPU-cycle points:
    // 4-step: 3729, 7457, 11186, 14916 (IRQ on last if not inhibited)
    // 5-step: 3729, 7457, 11186, 14916, 18640 (no frame IRQ)
    //
    // We’ll use cpu_cycle modulo the sequence length.

    if (frame_mode == 0) {
        // 4-step sequence length ~14916 CPU cycles
        uint32_t step = (uint32_t)(cpu_cycle % 14916);

        if (step == 3729)  quarterFrame();
        if (step == 7457)  { quarterFrame(); halfFrame(); }
        if (step == 11186) quarterFrame();
        if (step == 14915) {
            // last tick
            quarterFrame(); halfFrame();
            if (!irq_inhibit) frame_irq = true;
        }
    } else {
        // 5-step sequence length ~18640 CPU cycles
        uint32_t step = (uint32_t)(cpu_cycle % 18640);

        if (step == 3729)  quarterFrame();
        if (step == 7457)  { quarterFrame(); halfFrame(); }
        if (step == 11186) quarterFrame();
        if (step == 14916) { quarterFrame(); halfFrame(); }
        // step == 18639 ends; no IRQ in 5-step
    }
}

uint8_t apu::pulseOutput(const Pulse& p) const {
    if (!p.enabled) return 0;
    if (p.length_counter == 0) return 0;

    // Silencing rules: timer < 8 is silent on real APU (ultrasonic / invalid)
    if (p.timer < 8) return 0;

    // Duty patterns (8-step)
    static constexpr uint8_t dutyTable[4][8] = {
        {0,1,0,0,0,0,0,0}, // 12.5%
        {0,1,1,0,0,0,0,0}, // 25%
        {0,1,1,1,1,0,0,0}, // 50%
        {1,0,0,1,1,1,1,1}  // 25% negated
    };

    uint8_t seqBit = dutyTable[p.duty][p.seq_step & 7];
    if (seqBit == 0) return 0;

    uint8_t env = p.constant_volume ? p.volume : p.env_decay;
    return env & 0x0F;
}

void apu::clock() {
    cpu_cycle++;

    // Frame sequencer events
    clockFrameSequencer();

    // Pulse timer/sequencer runs at CPU rate
    // On each CPU cycle, decrement timer counter; when hits 0, reload and advance sequence.
    if (p1.timer_counter == 0) {
        p1.timer_counter = p1.timer;
        p1.seq_step = (p1.seq_step + 1) & 7;
    } else {
        p1.timer_counter--;
    }

    if (p2.timer_counter == 0) {
        p2.timer_counter = p2.timer;
        p2.seq_step = (p2.seq_step + 1) & 7;
    } else {
        p2.timer_counter--;
    }
    // ---- audio sample generation ----
    // We are clocked at CPU rate. Convert CPU cycles -> audio samples.
    m_samplePhase += (double)m_sampleRate / CPU_HZ;

    while (m_samplePhase >= 1.0) {
        m_samplePhase -= 1.0;
        pushSample(sample()); // sample() you already have (Pulse 1 mix)
    }
}

float apu::sample() const {
    // Right now: Pulse 1 only.
    // Use NES’s nonlinear mixer for pulse channels (even if only one is used).
    //
    // pulse_out = 95.88 / (8128/(p1+p2) + 100)
    // If p1+p2 == 0 -> 0
    uint8_t p1o = pulseOutput(p1);
    uint8_t p2o = pulseOutput(p2);


    int pulseSum = (int)p1o + (int)p2o;
    if (pulseSum == 0) return 0.0f;

    float out = 95.88f / ((8128.0f / (float)pulseSum) + 100.0f);

    // Keep in [-1,1] style range; this is already 0..~0.3
    return out;
}


void apu::setSampleRate(uint32_t hz) {
    if (hz == 0) hz = 48000;
    m_sampleRate = hz;
    m_samplePhase = 0.0;

    // optional: clear buffer on rate change
    m_audioWrite.store(0, std::memory_order_relaxed);
    m_audioRead.store(0, std::memory_order_relaxed);
}

uint32_t apu::availableSamples() const {
    uint32_t w = m_audioWrite.load(std::memory_order_acquire);
    uint32_t r = m_audioRead.load(std::memory_order_acquire);
    return w - r;
}

void apu::pushSample(float s) {
    uint32_t w = m_audioWrite.load(std::memory_order_relaxed);
    uint32_t r = m_audioRead.load(std::memory_order_acquire);

    // If full, drop the oldest sample (advance read).
    if ((w - r) >= AUDIO_RING_SIZE) {
        m_audioRead.store(r + 1, std::memory_order_release);
    }

    m_audioRing[w & AUDIO_RING_MASK] = s;
    m_audioWrite.store(w + 1, std::memory_order_release);
}

uint32_t apu::popSamples(float* out, uint32_t frames) {
    uint32_t r = m_audioRead.load(std::memory_order_relaxed);
    uint32_t w = m_audioWrite.load(std::memory_order_acquire);

    uint32_t avail = w - r;
    uint32_t toRead = (frames < avail) ? frames : avail;

    for (uint32_t i = 0; i < toRead; i++) {
        out[i] = m_audioRing[(r + i) & AUDIO_RING_MASK];
    }

    m_audioRead.store(r + toRead, std::memory_order_release);
    return toRead;
}
