#include "drawing/framebuffer.hpp"
#include "drawing/rendering/shader.hpp"
#include "tools/config_default.hpp"
#include "tools/hardwareconfig.hpp"
#include "tools/devices.hpp"
#include <math.h>


bool FrameBuffer::Allocate(){
    if (pixels != nullptr || sideBuffer != nullptr){
        return false;
    }
    pixels = (uint16_t*)ps_malloc(sizeof(uint16_t) * CANVAS_HEIGHT * CANVAS_WIDTH);
    if (pixels == nullptr){
        return false;
    }
    sideBuffer = (uint16_t*)ps_malloc(sizeof(uint16_t) * CANVAS_HEIGHT * CANVAS_WIDTH);
    if (sideBuffer == nullptr){
        free(pixels);
        pixels = nullptr;
        return false;
    }
    sizeX = CANVAS_WIDTH;
    sizeY = CANVAS_HEIGHT;
    return true;
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

static inline uint16_t sampleFrameBuffer(const uint16_t *buffer, int x, int y, int width, int height){
    return (x < 0 || y < 0 || x >= width || y >= height) ? 0 : buffer[y * width + x];
}

void FrameBuffer::DrawFrameBuffer(FlipConfig &flipSettings, ShaderType shader, float shaderStrenght){
    // Apply the shader before the interpolation pass so the idle breathing
    // movement affects the final rendered face, not the source frame cache.
    for (int16_t y = 0; y < sizeY; y++) {
        for (int16_t x = 0; x < sizeX; x++) {
            uint8_t r, g, b;
            Devices::Display->color565to888(pixels[y * sizeX + x], r, g, b);
            ShaderProcessor::UpdateColorByShader(x, y, r, g, b, shader, shaderStrenght, this);
            sideBuffer[y * sizeX + x] = Devices::Display->color565(r, g, b);
        }
    }

    const int offsetXInteger = (int)floorf(offsetX);
    const int offsetYInteger = (int)floorf(offsetY);
    const int offsetXFraction = (int)((offsetX - offsetXInteger) * 256.0f + 0.5f);
    const int offsetYFraction = (int)((offsetY - offsetYInteger) * 256.0f + 0.5f);
    const int weightTopLeft = (256 - offsetXFraction) * (256 - offsetYFraction);
    const int weightTopRight = offsetXFraction * (256 - offsetYFraction);
    const int weightBottomLeft = (256 - offsetXFraction) * offsetYFraction;
    const int weightBottomRight = offsetXFraction * offsetYFraction;

    for (int16_t y = 0; y < sizeY; y++) {
        for (int16_t x = 0; x < sizeX; x++) {
            const int sourceX = x - offsetXInteger;
            const int sourceY = y - offsetYInteger;
            const uint16_t topLeft = sampleFrameBuffer(sideBuffer, sourceX, sourceY, sizeX, sizeY);
            const uint16_t topRight = sampleFrameBuffer(sideBuffer, sourceX - 1, sourceY, sizeX, sizeY);
            const uint16_t bottomLeft = sampleFrameBuffer(sideBuffer, sourceX, sourceY - 1, sizeX, sizeY);
            const uint16_t bottomRight = sampleFrameBuffer(sideBuffer, sourceX - 1, sourceY - 1, sizeX, sizeY);

            auto blendChannel = [&](int shift, int mask) {
                return (int)((((topLeft >> shift) & mask) * weightTopLeft +
                              ((topRight >> shift) & mask) * weightTopRight +
                              ((bottomLeft >> shift) & mask) * weightBottomLeft +
                              ((bottomRight >> shift) & mask) * weightBottomRight + 32768) >> 16);
            };

            const int r5 = blendChannel(11, 0x1F);
            const int g6 = blendChannel(5, 0x3F);
            const int b5 = blendChannel(0, 0x1F);
            Devices::Display->setPixelWithFlip(x, y,
                (r5 << 3) | (r5 >> 2),
                (g6 << 2) | (g6 >> 4),
                (b5 << 3) | (b5 >> 2),
                flipSettings);
        }
    }

    offsetY = sinf((float)millis() / 1000.0f) * 2.0f;
    offsetX = sinf((float)millis() / 2000.0f);
}
