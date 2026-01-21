#include "header/GLTextures.h"

// GLAD
#include "external/glad/include/glad/glad.h"

void GLTextures::init()
{
    // Framebuffer texture (256x240)
    glGenTextures(1, &framebufferTex);
    glBindTexture(GL_TEXTURE_2D, framebufferTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D, 0,
        GL_RGBA,
        256, 240,
        0,
        GL_BGRA, GL_UNSIGNED_BYTE,
        nullptr
    );

    // Pattern textures (128x128 each)
    glGenTextures(2, patternTexs);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, patternTexs[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexImage2D(
            GL_TEXTURE_2D, 0,
            GL_RGBA,
            128, 128,
            0,
            GL_BGRA, GL_UNSIGNED_BYTE,
            nullptr
        );
    }
}

void GLTextures::shutdown()
{
    if (framebufferTex) {
        glDeleteTextures(1, &framebufferTex);
        framebufferTex = 0;
    }
    glDeleteTextures(2, patternTexs);
    patternTexs[0] = patternTexs[1] = 0;
}

void GLTextures::uploadFrameBGRA(const uint32_t* bgra256x240)
{
    glBindTexture(GL_TEXTURE_2D, framebufferTex);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0,
        0, 0, 256, 240,
        GL_BGRA, GL_UNSIGNED_BYTE,
        bgra256x240
    );
}

void GLTextures::uploadPatternBGRA(int index0or1, const uint32_t* bgra128x128)
{
    if (index0or1 < 0 || index0or1 > 1) return;

    glBindTexture(GL_TEXTURE_2D, patternTexs[index0or1]);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0,
        0, 0, 128, 128,
        GL_BGRA, GL_UNSIGNED_BYTE,
        bgra128x128
    );
}
