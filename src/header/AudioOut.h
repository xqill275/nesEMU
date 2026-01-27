// header/AudioOut.h
#pragma once
#include <cstdint>

class apu;

class AudioOut {
public:
    bool init(apu* a, uint32_t sampleRate = 48000);
    void shutdown();

    apu* m_apu = nullptr;

    // Opaque miniaudio types (defined in .cpp)
    struct Impl;
    Impl* impl = nullptr;
};
