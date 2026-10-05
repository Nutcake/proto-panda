#pragma once
#include <stdint.h>
#include "tools/sectionview.hpp"
#include "drawing/rendering/shadertypes.hpp"
#include "tools/displays.hpp"


class FrameBuffer{
    public:
        FrameBuffer():sizeX(0),sizeY(0),pixels(nullptr){};
        bool Allocate();    
        void ClearFrameBuffer();
        void DrawFrameBuffer(FlipConfig &flipSettings, ShaderType shader, float shaderStrenght);
        inline void SetPixel(int x,int y, uint16_t c){
            pixels[x + y * sizeX] = c; 
        };
        inline uint16_t GetSizeX(){
            return sizeX;
        }
        inline uint16_t GetSizeY(){
            return sizeY;
        }
        inline uint16_t GetPixel(uint16_t x, uint16_t y){
            if (x >= sizeX || x < 0){
                return 0;
            }
            if (y >= sizeY || y < 0){
                return 0;
            }
            return pixels[x + y * sizeX];
        }
        inline void SetPixelSafe(int16_t x,int16_t y, uint16_t c){
            if (x >=  sizeX || x < 0){
                return;
            }
            if (y >= sizeY || y < 0){
                return;
            }
            pixels[x + y * sizeX] = c; 
        };
    private:
        uint16_t sizeX, sizeY;
        uint16_t *pixels;
        uint16_t *sideBuffer;
        SectionMap<1> view;
        float offsetX, offsetY;
        uint64_t lastDrawTime;
};

extern FrameBuffer g_frameBuffer;