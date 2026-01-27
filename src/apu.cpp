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
    tri = {};
    noise = {};
    noise.lfsr = 1;
    dmc = {};

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
    if (tri.length_counter > 0) s |= (1 << 2);
    if (noise.length_counter > 0) s |= (1 << 3);
    if (dmc.bytes_remaining > 0) s |= (1 << 4);

    if (dmc.irq) s |= (1 << 7);
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
        if (tri.length_counter > 0) s |= (1 << 2);
        if (noise.length_counter > 0) s |= (1 << 3);
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
        tri.enabled = (data & 0x04) != 0;
        noise.enabled = (data & 0x08) != 0;
        dmc.enabled = (data & 0x10) != 0;

        if (!p1.enabled) p1.length_counter = 0;
        if (!p2.enabled) p2.length_counter = 0;
        if (!tri.enabled) tri.length_counter = 0;
        if (!noise.enabled) noise.length_counter = 0;

        if (!dmc.enabled) {
            dmc.bytes_remaining = 0;
            dmc.sample_buffer_empty = true;
            dmc.bits_remaining = 0;
            dmc.irq = false;
        } else {
            // If enabling and nothing queued, start a new sample
            if (dmc.bytes_remaining == 0) {
                dmc.current_addr = 0xC000u + (uint16_t)dmc.sample_addr_reg * 64u;
                dmc.bytes_remaining = (uint16_t)dmc.sample_len_reg * 16u + 1u;
            }
        }

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

    // -------- Triangle registers ($4008-$400B) --------
    if (addr == 0x4008) {
        tri.control_flag  = (data & 0x80) != 0;
        tri.linear_reload = data & 0x7F;
        return;
    }

    if (addr == 0x4009) {
        // Unused on NES APU (still mirrored)
        return;
    }

    if (addr == 0x400A) {
        tri.timer = (tri.timer & 0xFF00) | data;
        return;
    }

    if (addr == 0x400B) {
        tri.timer = (tri.timer & 0x00FF) | ((uint16_t)(data & 0x07) << 8);

        // Load length counter if enabled
        uint8_t len_idx = (data >> 3) & 0x1F;
        if (tri.enabled) {
            tri.length_counter = lengthTable(len_idx);
        }

        // Writing $400B sets the linear reload flag
        tri.linear_reload_flag = true;

        return;
    }


    // -------- Noise registers ($400C-$400F) --------
    if (addr == 0x400C) {
        noise.length_halt      = (data & 0x20) != 0;
        noise.constant_volume  = (data & 0x10) != 0;
        noise.volume           = data & 0x0F;
        noise.env_start        = true;
        return;
    }

    if (addr == 0x400D) {
        // unused on NES
        return;
    }

    if (addr == 0x400E) {
        noise.mode   = (data & 0x80) != 0;
        noise.period = data & 0x0F;
        return;
    }

    if (addr == 0x400F) {
        uint8_t len_idx = (data >> 3) & 0x1F;
        if (noise.enabled) {
            noise.length_counter = lengthTable(len_idx);
        }
        noise.env_start = true;
        return;
    }

    // -------- DMC registers ($4010-$4013) --------
    if (addr == 0x4010) {
        dmc.irq_enable = (data & 0x80) != 0;
        dmc.loop       = (data & 0x40) != 0;
        dmc.rate       = data & 0x0F;

        if (!dmc.irq_enable) dmc.irq = false; // disabling IRQ clears it
        return;
    }

    if (addr == 0x4011) {
        dmc.output_level = data & 0x7F;
        return;
    }

    if (addr == 0x4012) {
        dmc.sample_addr_reg = data;
        return;
    }

    if (addr == 0x4013) {
        dmc.sample_len_reg = data;
        return;
    }
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
    clockLinearCounter(tri);
    clockEnvelopeNoise(noise);
}

void apu::halfFrame() {
    clockLengthCounter(p1);
    clockLengthCounter(p2);
    clockLengthCounterNoise(noise);

    if (!tri.control_flag && tri.length_counter > 0) {
        tri.length_counter--;
    }
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
    clockDMC();

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

    if (tri.timer_counter == 0) {
        tri.timer_counter = tri.timer;

        // Triangle advances only when it is “audible” (linear + length nonzero)
        if (tri.length_counter > 0 && tri.linear_counter > 0) {
            tri.seq_step = (tri.seq_step + 1) & 31;
        }
    } else {
        tri.timer_counter--;
    }

    // Noise timer/LFSR
    if (noise.timer_counter == 0) {
        noise.timer_counter = noisePeriodTable(noise.period);

        // Update LFSR if channel is potentially active
        if (noise.enabled && noise.length_counter > 0) {
            // feedback bit uses bit0 XOR bit1 (mode=0) or bit0 XOR bit6 (mode=1)
            uint16_t bit0 = noise.lfsr & 0x0001;
            uint16_t tap  = noise.mode ? ((noise.lfsr >> 6) & 0x0001)
                                       : ((noise.lfsr >> 1) & 0x0001);
            uint16_t feedback = bit0 ^ tap;

            noise.lfsr >>= 1;
            noise.lfsr |= (feedback << 14); // keep 15-bit register
        }
    } else {
        noise.timer_counter--;
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
    // ----- Pulse mixer -----
    uint8_t p1o = pulseOutput(p1);
    uint8_t p2o = pulseOutput(p2);
    int pulseSum = (int)p1o + (int)p2o;

    float pulseOut = 0.0f;
    if (pulseSum != 0) {
        pulseOut = 95.88f / ((8128.0f / (float)pulseSum) + 100.0f);
    }

    // ----- TND mixer -----
    float t = (float)triangleOutput(tri);      // 0..15
    float n = (float)noiseOutput(noise);       // 0..15
    float d = (float)dmcOutput(); // 0..127

    float tndOut = 0.0f;
    float denom = (t / 8227.0f) + (n / 12241.0f) + (d / 22638.0f);
    if (denom > 0.0f) {
        tndOut = 159.79f / ((1.0f / denom) + 100.0f);
    }

    return pulseOut + tndOut;
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

void apu::clockLinearCounter(Triangle& t) {
    if (t.linear_reload_flag) {
        t.linear_counter = t.linear_reload;
    } else if (t.linear_counter > 0) {
        t.linear_counter--;
    }

    // If control_flag is clear, reload flag is cleared after the clock
    if (!t.control_flag) {
        t.linear_reload_flag = false;
    }
}

uint8_t apu::triangleOutput(const Triangle& t) const {
    if (!t.enabled) return 0;
    if (t.length_counter == 0) return 0;
    if (t.linear_counter == 0) return 0;

    // Very small timer values produce ultrasonic / invalid output; commonly muted
    if (t.timer < 2) return 0;

    // 32-step triangle sequence (0..15..0..15..)
    static constexpr uint8_t seq[32] = {
        15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
    };

    return seq[t.seq_step & 31];
}

void apu::clockEnvelopeNoise(Noise& n) {
    if (n.env_start) {
        n.env_start  = false;
        n.env_decay  = 15;
        n.env_divider = n.volume;
    } else {
        if (n.env_divider == 0) {
            n.env_divider = n.volume;

            if (n.env_decay == 0) {
                if (n.length_halt) {
                    n.env_decay = 15; // loop
                }
            } else {
                n.env_decay--;
            }
        } else {
            n.env_divider--;
        }
    }
}

void apu::clockLengthCounterNoise(Noise& n) {
    if (!n.length_halt && n.length_counter > 0) {
        n.length_counter--;
    }
}

uint16_t apu::noisePeriodTable(uint8_t idx) {
    // NTSC noise periods (CPU cycles per LFSR shift)
    // Common table used by most emulators.
    static constexpr uint16_t t[16] = {
        4, 8, 16, 32, 64, 96, 128, 160,
        202, 254, 380, 508, 762, 1016, 2034, 4068
    };
    return t[idx & 0x0F];
}

uint8_t apu::noiseOutput(const Noise& n) const {
    if (!n.enabled) return 0;
    if (n.length_counter == 0) return 0;

    // If LFSR bit0 is 1, output is forced to 0 (silence) on NES noise
    if (n.lfsr & 0x0001) return 0;

    uint8_t env = n.constant_volume ? n.volume : n.env_decay;
    return env & 0x0F;
}

uint16_t apu::dmcRateTable(uint8_t idx) {
    // NTSC DMC rates (in CPU cycles per bit)
    static constexpr uint16_t t[16] = {
        428, 380, 340, 320, 286, 254, 226, 214,
        190, 160, 142, 128, 106,  85,  72,  54
    };
    return t[idx & 0x0F];
}

void apu::refillDmcSampleBuffer() {
    if (!dmc.enabled) return;
    if (!dmc.sample_buffer_empty) return;
    if (dmc.bytes_remaining == 0) return;

    if (!m_dmcRead) {
        // no bus hook yet; stay silent
        return;
    }

    // Fetch one byte from CPU memory
    dmc.sample_buffer = m_dmcRead(dmc.current_addr);
    dmc.sample_buffer_empty = false;

    // Increment address (wrap at 0xFFFF -> 0x8000)
    dmc.current_addr++;
    if (dmc.current_addr == 0x0000) dmc.current_addr = 0x8000;

    // Decrement remaining
    dmc.bytes_remaining--;

    // End of sample handling
    if (dmc.bytes_remaining == 0) {
        if (dmc.loop) {
            dmc.current_addr = 0xC000u + (uint16_t)dmc.sample_addr_reg * 64u;
            dmc.bytes_remaining = (uint16_t)dmc.sample_len_reg * 16u + 1u;
        } else if (dmc.irq_enable) {
            dmc.irq = true;
        }
    }
}

void apu::clockDMC() {
    // Refill sample buffer ASAP when empty and data remains
    if (dmc.sample_buffer_empty) {
        refillDmcSampleBuffer();
    }

    if (dmc.timer_counter == 0) {
        dmc.timer_counter = dmcRateTable(dmc.rate);

        // If we have no bits loaded, try to load them from sample buffer
        if (dmc.bits_remaining == 0) {
            if (!dmc.sample_buffer_empty) {
                dmc.shift_reg = dmc.sample_buffer;
                dmc.sample_buffer_empty = true;
                dmc.bits_remaining = 8;
            } else {
                // No data, output holds steady
                return;
            }
        }

        // Output unit: process 1 bit
        uint8_t bit = dmc.shift_reg & 0x01;
        dmc.shift_reg >>= 1;
        dmc.bits_remaining--;

        if (bit) {
            if (dmc.output_level <= 125) dmc.output_level += 2;
        } else {
            if (dmc.output_level >= 2) dmc.output_level -= 2;
        }

    } else {
        dmc.timer_counter--;
    }
}

uint8_t apu::dmcOutput() const {
    // DMC output is the DAC level 0..127
    return dmc.output_level & 0x7F;
}

