// header/apu.h
#pragma once
#include <cstdint>
#include <cmath>
#include <atomic>
#include <array>
#include <functional>

class apu {
public:
    apu() = default;

    void reset();

    // CPU memory-mapped interface
    uint8_t cpuRead(uint16_t addr, bool readonly = false);
    void    cpuWrite(uint16_t addr, uint8_t data);

    // Tick APU at CPU clock rate (once per CPU cycle)
    void clock();

    // Produce a mono sample (no resampling here yet; just current mixed level)
    float sample() const;

    // Debug helpers (for your panel)
    uint8_t debugReg(uint16_t addr) const;
    uint8_t debugStatus4015() const;
    bool    debugFrameIRQ() const { return frame_irq; }

    void setSampleRate(uint32_t hz);
    uint32_t sampleRate() const { return m_sampleRate; }

    // Called from audio thread (miniaudio callback)
    uint32_t popSamples(float* out, uint32_t frames);

    void setDmcReader(std::function<uint8_t(uint16_t)> fn) { m_dmcRead = std::move(fn); }

private:
    // iNES / NES APU base clock (NTSC)
    static constexpr double CPU_HZ = 1789773.0;

    // Raw register mirror ($4000-$4017)
    uint8_t reg[0x18] = {}; // index = addr - 0x4000

    // Frame counter ($4017)
    uint8_t frame_mode = 0;        // 0=4-step, 1=5-step
    bool    irq_inhibit = false;
    bool    frame_irq = false;

    // Internal cycle counter
    uint64_t cpu_cycle = 0;

    // -------- Pulse 1 channel --------
    struct Pulse {
        bool enabled = false;

        // $4000
        uint8_t duty = 0;          // 0..3
        bool    length_halt = false; // also envelope loop
        bool    constant_volume = false;
        uint8_t volume = 0;        // 0..15

        // Envelope
        uint8_t env_divider = 0;
        uint8_t env_decay = 0;
        bool    env_start = false;

        // Timer ($4002/$4003)
        uint16_t timer = 0;        // 11-bit
        uint16_t timer_counter = 0;

        // Sequencer
        uint8_t seq_step = 0;      // 0..7

        // Length counter
        uint8_t length_counter = 0;

        // For now we ignore sweep ($4001) (add later)
    } p1, p2;

    // -------- Triangle channel --------
    struct Triangle {
        bool enabled = false;

        // $4008
        bool    control_flag = false;   // also length counter halt
        uint8_t linear_reload = 0;      // 0..127

        // Linear counter
        uint8_t linear_counter = 0;
        bool    linear_reload_flag = false;

        // Timer ($400A/$400B)
        uint16_t timer = 0;             // 11-bit
        uint16_t timer_counter = 0;

        // Sequencer (32-step)
        uint8_t seq_step = 0;           // 0..31

        // Length counter
        uint8_t length_counter = 0;
    } tri;

    // -------- Noise channel --------
    struct Noise {
        bool enabled = false;

        // $400C
        bool    length_halt = false;     // also envelope loop
        bool    constant_volume = false;
        uint8_t volume = 0;              // 0..15

        // Envelope
        uint8_t env_divider = 0;
        uint8_t env_decay = 0;
        bool    env_start = false;

        // $400E
        bool    mode = false;            // 0=long (bit1), 1=short (bit6)
        uint8_t period = 0;              // 0..15

        // Timer
        uint16_t timer_counter = 0;

        // LFSR (15-bit). Bit0 is output (0 = audible, 1 = silent on real hw mixing logic)
        uint16_t lfsr = 1;

        // $400F length load
        uint8_t length_counter = 0;
    } noise;

    // -------- DMC channel --------
    struct DMC {
        bool enabled = false;

        // $4010
        bool irq_enable = false;
        bool loop = false;
        uint8_t rate = 0;           // 0..15

        // $4011
        uint8_t output_level = 0;   // 0..127 (DAC)

        // $4012/$4013
        uint8_t sample_addr_reg = 0;   // base address = 0xC000 + (reg * 64)
        uint8_t sample_len_reg  = 0;   // length = (reg * 16) + 1 bytes

        // Playback state
        uint16_t current_addr = 0;
        uint16_t bytes_remaining = 0;

        uint8_t  shift_reg = 0;
        uint8_t  bits_remaining = 0; // 0..8

        uint8_t  sample_buffer = 0;
        bool     sample_buffer_empty = true;

        uint16_t timer_counter = 0;

        bool irq = false;
    } dmc;


    // --- Audio output buffer (SPSC ring buffer) ---
    static constexpr uint32_t AUDIO_RING_SIZE = 1u << 15; // 32768 samples
    static constexpr uint32_t AUDIO_RING_MASK = AUDIO_RING_SIZE - 1;

    std::array<float, AUDIO_RING_SIZE> m_audioRing{};
    std::atomic<uint32_t> m_audioWrite{0};
    std::atomic<uint32_t> m_audioRead{0};

    uint32_t m_sampleRate = 48000;
    double   m_samplePhase = 0.0; // fractional accumulator

    void pushSample(float s);
    uint32_t availableSamples() const;

private:
    std::function<uint8_t(uint16_t)> m_dmcRead;
    inline uint8_t& R(uint16_t addr) { return reg[addr - 0x4000]; }
    inline uint8_t  R(uint16_t addr) const { return reg[addr - 0x4000]; }

    void clockFrameSequencer();
    void quarterFrame(); // envelopes
    void halfFrame();    // length counters (and sweep later)

    void clockEnvelope(Pulse& p);
    void clockLengthCounter(Pulse& p);

    void clockEnvelopeNoise(Noise& n);
    void clockLengthCounterNoise(Noise& n);

    void clockLinearCounter(Triangle& t);
    uint8_t triangleOutput(const Triangle& t) const;

    uint8_t noiseOutput(const Noise& n) const;
    static uint16_t noisePeriodTable(uint8_t idx);

    uint8_t pulseOutput(const Pulse& p) const;

    static uint8_t lengthTable(uint8_t idx);

    static uint16_t dmcRateTable(uint8_t idx);
    void clockDMC();
    uint8_t dmcOutput() const;
    void refillDmcSampleBuffer();
};
