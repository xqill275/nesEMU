#pragma once
#include <cstdint>

using GLuint = unsigned int;

class GLTextures {
public:
    void init();
    void shutdown();

    void uploadFrameBGRA(const uint32_t* bgra256x240);
    void uploadPatternBGRA(int index0or1, const uint32_t* bgra128x128);

    GLuint frameTex() const { return framebufferTex; }
    GLuint patternTex(int i) const { return patternTexs[i]; }

private:
    GLuint framebufferTex = 0;
    GLuint patternTexs[2] = {0, 0};
};
