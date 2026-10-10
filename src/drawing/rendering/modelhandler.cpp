#include "drawing/rendering/modelhandler.hpp"
#include "tools/devices.hpp"
#include "tools/oledscreen.hpp"
#include "drawing/framebuffer.hpp"
#include "drawing/animation.hpp"

void ModelHandler::RenderModels(std::vector<Model*> mdls){
    
    for (auto& model : mdls) {
        if (model->triangleCount == 0 || model->visible == false) continue;
        for (int i = model->triangleCount - 1; i >= 0; i--) {
            model->RasterTriangleWithBitmap(this, i, g_frameBuffer);
        }
    }
}


void ModelHandler::RenderScene(std::vector<Model*> mdls, ShaderType shader, float shaderStrenght){
    Devices::Display->startWrite();
    memset(pixelBitmap, 0,  CANVAS_HEIGHT * (CANVAS_WIDTH/8) * sizeof(uint8_t));
    g_frameBuffer.ClearFrameBuffer();
    RenderModels(mdls);
    g_frameBuffer.DrawFrameBuffer(FlipConfig::DefaultFlipConfig, shader, shaderStrenght, g_animation.isBreathingEnabled());
    Devices::Display->endWrite();
}
