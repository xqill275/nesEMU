// src/AudioOut.cpp
#include "header/AudioOut.h"
#include "header/apu.h"

#define MINIAUDIO_IMPLEMENTATION
#include "external/miniaudio/miniaudio.h"

struct AudioOut::Impl {
    ma_device device{};
};

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;

    AudioOut* self = (AudioOut*)pDevice->pUserData;
    float* out = (float*)pOutput;

    // Default to silence
    for (ma_uint32 i = 0; i < frameCount; i++) out[i] = 0.0f;

    if (!self || !self->impl || !self->m_apu) return;

    // Pull what we have; if underflow, remainder stays silent
    ma_uint32 got = self->m_apu->popSamples(out, frameCount);
    (void)got;
}

bool AudioOut::init(apu* a, uint32_t sampleRate) {
    m_apu = a;
    if (!m_apu) return false;

    impl = new Impl();

    m_apu->setSampleRate(sampleRate);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate        = sampleRate;
    config.dataCallback      = data_callback;
    config.pUserData         = this;

    if (ma_device_init(nullptr, &config, &impl->device) != MA_SUCCESS) {
        delete impl; impl = nullptr;
        return false;
    }

    if (ma_device_start(&impl->device) != MA_SUCCESS) {
        ma_device_uninit(&impl->device);
        delete impl; impl = nullptr;
        return false;
    }

    return true;
}

void AudioOut::shutdown() {
    if (impl) {
        ma_device_uninit(&impl->device);
        delete impl;
        impl = nullptr;
    }
    m_apu = nullptr;
}
