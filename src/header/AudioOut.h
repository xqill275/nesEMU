// header/AudioOut.h

class apu;
#pragma once
#include <cstdint>

class AudioOut {
public:
    bool init(apu* a, uint32_t sampleRate = 48000);
    void shutdown();

    apu* m_apu = nullptr;


    struct Impl;
    Impl* impl = nullptr;
};
