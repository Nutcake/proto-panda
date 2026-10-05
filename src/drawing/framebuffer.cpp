#include "drawing/framebuffer.hpp"
#include "drawing/rendering/shader.hpp"
#include "tools/config_default.hpp"
#include "tools/hardwareconfig.hpp"
#include "tools/devices.hpp"

bool FrameBuffer::Allocate(){
    if (pixels != nullptr){
        return false;
    }
    pixels = (uint16_t *)ps_malloc(sizeof(uint16_t) * CANVAS_HEIGHT * CANVAS_WIDTH);
    sideBuffer = (uint16_t *)ps_malloc(sizeof(uint16_t) * CANVAS_HEIGHT * CANVAS_WIDTH);
    sizeX = CANVAS_WIDTH;
    sizeY = CANVAS_HEIGHT;
    return pixels != nullptr;
}

void FrameBuffer::ClearFrameBuffer(){
    uint8_t r, g, b;
    int16_t y,x;
    int pixelId = 0;
    for (y = 0; y < sizeY; y++) {
        pixelId = y * sizeX;
        for (x = 0; x < sizeX; x++) {
            pixels[pixelId + x] = 0;
        }
    }
}

static inline uint16_t Sample(const uint16_t *buf, int x, int y, int w, int h)
{
    return (x < 0 || y < 0 || x >= w || y >= h) ? 0 : buf[y * w + x];
}

void FrameBuffer::DrawFrameBuffer(FlipConfig &flipSettings, ShaderType shader, float shaderStrength)
{
    for (int16_t y = 0; y < sizeY; y++)
    {
        for (int16_t x = 0; x < sizeX; x++)
        {
            uint8_t r, g, b;
            Devices::Display->color565to888(pixels[y * sizeX + x], r, g, b);
            ShaderProcessor::UpdateColorByShader(x, y, r, g, b, shader, shaderStrength, this);
            sideBuffer[y * sizeX + x] = Devices::Display->color565(r, g, b);
        }
    }

    const int ix = (int)floorf(offsetX);
    const int iy = (int)floorf(offsetY);
    const int fx = (int)((offsetX - ix) * 256.0f + 0.5f); 
    const int fy = (int)((offsetY - iy) * 256.0f + 0.5f);
    const int w00 = (256 - fx) * (256 - fy);
    const int w10 = fx * (256 - fy);
    const int w01 = (256 - fx) * fy;
    const int w11 = fx * fy; 

    for (int y = 0; y < sizeY; y++)
    {
        for (int x = 0; x < sizeX; x++)
        {
            const int sx = x - ix, sy = y - iy;
            const uint16_t c00 = Sample(sideBuffer, sx, sy, sizeX, sizeY);
            const uint16_t c10 = Sample(sideBuffer, sx - 1, sy, sizeX, sizeY);
            const uint16_t c01 = Sample(sideBuffer, sx, sy - 1, sizeX, sizeY);
            const uint16_t c11 = Sample(sideBuffer, sx - 1, sy - 1, sizeX, sizeY);

            auto blend = [&](int shift, int mask)
            {
                return (int)((((c00 >> shift) & mask) * w00 + ((c10 >> shift) & mask) * w10 +
                              ((c01 >> shift) & mask) * w01 + ((c11 >> shift) & mask) * w11 + 32768) >>
                             16);
            };

            int r5 = blend(11, 0x1F), g6 = blend(5, 0x3F), b5 = blend(0, 0x1F);
            uint8_t r8 = (r5 << 3) | (r5 >> 2);
            uint8_t g8 = (g6 << 2) | (g6 >> 4);
            uint8_t b8 = (b5 << 3) | (b5 >> 2);

            Devices::Display->setPixelWithFlip(x, y, r8, g8, b8, flipSettings);
        }
    }
    offsetY = sinf((float)millis() / 1000.0f) * 2.f;
    offsetX = sinf((float)millis() / 2000.0f) * 1.f;
}