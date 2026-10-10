#include "drawing/animation.hpp"
#include "tools/oledscreen.hpp"
#include "tools/devices.hpp"
#include "tools/compression.hpp"
#include "tools/hardwareconfig.hpp"
#include "drawing/framerepository.hpp"
#include "drawing/modelanimation/keyframeplayer.hpp"
#include "tools/storage.hpp"
#include "drawing/rendering/shader.hpp"

#include "FS.h"
unsigned char* Animation::buffer = nullptr;





AnimationFrameAction AnimationSequence::InterruptFrame(int pinRead){ 
    if (pinRead == 0){
        m_frame++;
        int len = m_frames.size(); 
        if (m_frame > len-1){
            m_frame = 1;
        }
        return ANIMATION_FRAME_CHANGED;
    }else{
        if (m_frame != 0){
            m_frame = 0;
            return ANIMATION_FRAME_CHANGED;
        }
        return ANIMATION_NO_CHANGE;
    }
}

AnimationFrameAction AnimationSequence::ChangeFrame(){
    int len = m_frames.size(); 
    if (m_frame >= (len-1) ){
        m_frame = 0;
        if (m_repeat == -1){
            return ANIMATION_FRAME_CHANGED;
        }
        m_repeat--;
        if (m_repeat <= 0){
            return ANIMATION_FINISHED;
        }
        return ANIMATION_FRAME_CHANGED;
    }else{
        m_frame++;
        return ANIMATION_FRAME_CHANGED;
    }
}

void AnimationSequence::ResetIfNeeded(){
    if (m_isModel){
        g_kf.ResetSpecificAnimation(m_frame);
    }
}

AnimationFrameAction AnimationSequence::Update(uint32_t dt, int m_interruptPin, bool isManaged, ShaderType &shdr, float &strenght, bool randomBlinkEnabled){
    ShaderProcessor::IncrFrame();
    if (m_isModel){
        //Timing is handled here.
        g_kf.PlayAnimationId(m_frame, true, shdr, strenght);

        bool finishedAnimation = false;
        if (isManaged){
           finishedAnimation = g_kf.Update(dt);
        }
        if (finishedAnimation){
            if (m_repeat == -1){
                return ANIMATION_NEED_FLIP;
            }
            m_repeat--;
            if (m_repeat <= 0){
                return ANIMATION_FINISHED;
            }
        }
        return ANIMATION_NEED_FLIP;
    }

    if (m_isNew){
        m_counter = millis()+m_duration;
        m_isNew = false;
        return ANIMATION_FRAME_CHANGED;
    }

    if (!randomBlinkEnabled){
        m_freezeStartTime = 0;
    }

    if (randomBlinkEnabled && m_freezeFrame >= 0 && m_frame == m_freezeFrame){
        if (m_freezeStartTime == 0){
            m_freezeStartTime = millis();
            m_freezeDuration = m_freezeDurationMax > m_freezeDurationMin
                ? random(m_freezeDurationMin, m_freezeDurationMax)
                : max(m_freezeDurationMin, 0);
        }
        if (millis() - m_freezeStartTime >= m_freezeDuration){
            m_freezeStartTime = 0;
            return ChangeFrame();
        }
        return ANIMATION_NO_CHANGE;
    }

    if (m_counter <= millis()){
        m_counter = millis()+m_duration;
        if (m_interruptPin < 0){
            return ChangeFrame();
        }
        switch (m_updateMode){
        case 1:
            return InterruptFrame(digitalRead(m_interruptPin));
            break;
        default:
            return ChangeFrame();
        }
    }
    return ANIMATION_NO_CHANGE;
 
}
int AnimationSequence::GetFrameId(){
    if (m_isModel){
        return MODEL_FRAME_ID_OFFSET + m_frame;
    }
    return m_frames[m_frame];
}



void Animation::EnableFrameBuffer(bool enable){
    if (enable && m_frameBuffer == nullptr){
        m_frameBuffer = (uint16_t*)ps_malloc(sizeof(uint16_t) * CANVAS_WIDTH * CANVAS_HEIGHT);
    }
    m_copyToFrameBuffer = enable;
}

void Animation::LoadFrameAsTexture(int i){
    if (i == 0){
        return;
    }

    int32_t startPosition;
    int32_t flashFileLenght;
    
    g_frameRepo.getBulkOffsetByFrameId(i, startPosition, flashFileLenght);
    if (startPosition < 0){
        Serial.printf("Failed to find frame id %d, returned position %ld\n", i, startPosition);
        return;
    }

    File *file = g_frameRepo.takeFile();
    if (file == nullptr){
        return;
    }

    file->seek(startPosition);
    size_t rd = file->readBytes((char*)buffer, flashFileLenght);
    g_frameRepo.freeFile();

    if (rd != flashFileLenght){
        Serial.printf("Failed to read %d at pos %ld on file with size %ld. Read just %d bytes\n", i , startPosition, flashFileLenght, rd);
        g_frameRepo.freeFile();
        return;
    }

    uint8_t r, g, b;
    uint8_t version = buffer[0];
    if (version != PANDA_CACHE_VERSION){
        Serial.printf("Mismatched cached frame version. Current version is %d but in cache got %d. Clear the cache or rebuild bulk: %d", PANDA_CACHE_VERSION, version, buffer[1]);
        g_frameRepo.freeFile();
        return;
    }

    uint8_t flip_left = buffer[1];
    uint8_t flip_right = buffer[2];
    if (m_colorMode == 0){
        m_colorMode = (ColorMode)buffer[3];
    }

    if (m_texture == nullptr){
        m_texture = (uint16_t*)ps_malloc(sizeof(uint16_t) * CANVAS_WIDTH * CANVAS_HEIGHT);
        ShaderProcessor::SetTextureAddr(m_texture);
    }


    int byteIdOled = 0;
    int compressionMode = buffer[5];
    uint16_t fileLenght = *((uint16_t*)(&buffer[6])); 
    int16_t x=0;
    int16_t y=0;

    if (compressionMode == 1){
        int compressionReadPos = 0;
        uint8_t *readBuffer = (buffer+FILE_HEADER_BYTES);

        do{
            int lenght = readBuffer[compressionReadPos++];
            int iter = 1;
            
            if (lenght == 253){
                iter = readBuffer[compressionReadPos++];
                lenght = 1;
            }

            while(iter > 0){
                uint16_t color = readBuffer[compressionReadPos++]; 
                iter--;
                color |= (readBuffer[compressionReadPos++] << 8); 

                
                
                for (int iddx=0;iddx<lenght;iddx++){
                    m_texture[y * CANVAS_WIDTH + x] = color;
                    x++;
                    if (x >= CANVAS_WIDTH){
                        x = 0;
                        y++;
                        
                        if (y >= CANVAS_HEIGHT){
                            g_frameRepo.freeFile();
                            return;
                        }
                    }
                }
                if (compressionReadPos >= fileLenght){
                    g_frameRepo.freeFile();
                    return;
                }
                
            }
        } while(compressionReadPos <= fileLenght);
       
    }else{
        //Need to divide by 2 (size of uint16_t) because the buffer is converted to a uin16_t pixels.
        //And the header must be a multiple of 2 to avoid byte skipping
        uint16_t *readBuffer = (uint16_t *)(buffer);
        const int begin = FILE_HEADER_BYTES / sizeof(uint16_t);
        const int finish = begin+FILE_PIXEL_COUNT;

        for (int16_t idx=begin;idx<finish;idx++){
            uint16_t color = readBuffer[idx];
            
            m_texture[y * CANVAS_WIDTH + x] = color;
            x++;
            if (x >= CANVAS_WIDTH){
                x = 0;
                y++;
            }
        }
    }
    g_frameRepo.freeFile();
    return;
}

void Animation::Allocate(){
    Animation::buffer = (unsigned char*)ps_malloc(sizeof(unsigned char) * FILE_SIZE);
}

void Animation::drawFFTOverlay(FlipConfig flipSettings, int16_t frameId) {
    const int SCREEN_W = CANVAS_WIDTH;
    const int SCREEN_H = CANVAS_HEIGHT;
    const int COUNT    = g_fft.getBandCount(); 
    const int BAR_W    = SCREEN_W / COUNT; 
    
    for (int i = 0; i < COUNT; i++) {
        int barH = map(g_fft.getBandValue(i), 0, 200000, 0, SCREEN_H);
        barH = constrain(barH, 0, SCREEN_H);
        if (barH == 0) continue;
        
        uint8_t h = (uint8_t)((float)i / COUNT * 255.0f);
        uint8_t r, g, b;
        ShaderProcessor::Hsv2Rgb(h, 255, 255, r, g, b);
        
        for (int yOffset = 0; yOffset < barH; yOffset++) {
            int16_t y = SCREEN_H - barH + yOffset;

            for (int xOffset = 0; xOffset < BAR_W; xOffset++) {
                int16_t x = i * BAR_W + xOffset;

                uint16_t color = Devices::Display->color565(r, g, b);
                g_frameBuffer.SetPixel(x, y, color);
            }
        }
    }
}

void Animation::DrawFrame(int i){
    if (i == 0){
        return;
    }

    uint64_t ld = micros();
    uint64_t begin = ld;
    

    static int16_t frameId = 0;
    frameId++;    

    i--;
    
    int32_t startPosition;
    int32_t flashFileLenght;
    
    g_frameRepo.getBulkOffsetByFrameId(i, startPosition, flashFileLenght);
    if (startPosition < 0){
        Serial.printf("Failed to find frame id %d, returned position %ld\n", i, startPosition);
        return;
    }

    File *file = g_frameRepo.takeFile();
    if (file == nullptr){
        return;
    }

    file->seek(startPosition);
    size_t rd = file->readBytes((char*)buffer, flashFileLenght);
    g_frameRepo.freeFile();

    if (rd != flashFileLenght){
        Serial.printf("Failed to read %d at pos %ld on file with size %ld. Read just %d bytes\n", i , startPosition, flashFileLenght, rd);
        return;
    }

    m_frameLoadDuration = micros()-ld;
    ld = micros();

    auto currentModeLeft = m_colorMode;
    auto currentModeRight = m_colorMode;
    
    uint8_t r, g, b;
    uint8_t version = buffer[0];
    if (version != PANDA_CACHE_VERSION){
        Serial.printf("Mismatched cached frame version. Current version is %d but in cache got %d. Clear the cache or rebuild bulk: %d", PANDA_CACHE_VERSION, version, buffer[1]);
        return;
    }

    uint8_t flip_left = buffer[1];
    uint8_t flip_right = buffer[2];
    if (m_colorMode == 0){
        currentModeLeft = (ColorMode)buffer[3];
        currentModeRight = (ColorMode)buffer[4];
    }


    int byteIdOled = 0;
    int compressionMode = buffer[5];
    uint16_t fileLenght = *((uint16_t*)(&buffer[6])); 
    int16_t x=0;
    int16_t y=0;

    FlipConfig flipSettings(flip_left, flip_right, currentModeLeft, currentModeRight);

    Devices::Display->startWrite();
    if (compressionMode == 1){
        int compressionReadPos = 0;
        uint8_t *readBuffer = (buffer+FILE_HEADER_BYTES);

        do{
            int lenght = readBuffer[compressionReadPos++];
            int iter = 1;
            
            if (lenght == 253){
                iter = readBuffer[compressionReadPos++];
                lenght = 1;
            }

            while(iter > 0){
                uint16_t color = readBuffer[compressionReadPos++]; 
                iter--;
                color |= (readBuffer[compressionReadPos++] << 8); 

                
                for (int iddx=0;iddx<lenght;iddx++){
                    g_frameBuffer.SetPixel(x, y, color);
                    x++;
                    if (x >= CANVAS_WIDTH){
                        x = 0;
                        y++;
                        
                        if (y >= CANVAS_HEIGHT){
                            goto finished;
                        }
                    }
                }
                if (compressionReadPos >= fileLenght){
                    goto finished;
                }
                
            }
        } while(compressionReadPos <= fileLenght);
       
    }else{
        //Need to divide by 2 (size of uint16_t) because the buffer is converted to a uin16_t pixels.
        //And the header must be a multiple of 2 to avoid byte skipping
        uint16_t *readBuffer = (uint16_t *)(buffer);
        const int begin = FILE_HEADER_BYTES / sizeof(uint16_t);
        const int finish = begin+FILE_PIXEL_COUNT;

        for (int16_t idx=begin;idx<finish;idx++){
            uint16_t color = readBuffer[idx];

            g_frameBuffer.SetPixel(x, y, color);
            x++;
            if (x >= CANVAS_WIDTH){
                x = 0;
                y++;
            }
        }
    }
    finished:

    if (m_spritesInScene.size() > 0){
        xSemaphoreTake(m_SpriteMutex, portMAX_DELAY);
        for (auto &sp : m_spritesInScene){
            m_overlaySprites[sp]->DrawToFrameBuffer(g_frameBuffer);
        }
        xSemaphoreGive(m_SpriteMutex);
    }

    if (m_fftOverlay && g_fft.isRunning()){
        drawFFTOverlay(flipSettings, frameId);
    }

    g_frameBuffer.DrawFrameBuffer(flipSettings, m_shader, m_shaderStrenght, isBreathingEnabled());
    Devices::Display->endWrite();
    m_needFlip = true;
    m_frameDrawDuration = micros()-ld;
    m_cycleDuration =  micros()-begin;

}


void Animation::IncludeSpriteInPool(Sprite *s){
    int id = m_overlaySprites.size();
    s->id = id;
    m_overlaySprites.emplace_back(s);
}

int Animation::clearAllOverlaySprites()
{
    xSemaphoreTake(m_SpriteMutex, portMAX_DELAY);
    int count = (int)m_spritesInScene.size();
    m_spritesInScene.clear();
    xSemaphoreGive(m_SpriteMutex);
    return count;
}

bool Animation::setOverlaySprite(Sprite *s)
{
    if (s == nullptr){
        return false;
    }
    int id = s->GetId();
    xSemaphoreTake(m_SpriteMutex, portMAX_DELAY);
    if (id < 0 || id >= (int)m_overlaySprites.size())
    {
        xSemaphoreGive(m_SpriteMutex);
        return false;
    }
    Serial.printf("Added overlay ID: %d\n", id);
    m_spritesInScene.push_back(id);
    xSemaphoreGive(m_SpriteMutex);
    return true;
}

bool Animation::clearOverlaySprite(Sprite *s){
    if (s == nullptr){
        return false;
    }
    int id = s->GetId();
    xSemaphoreTake(m_SpriteMutex, portMAX_DELAY);

    for (auto it = m_spritesInScene.begin(); it != m_spritesInScene.end(); ++it)
    {
        if (*it == id)
        {
            Serial.printf("Removed overlay ID: %d\n", id);
            m_spritesInScene.erase(it);
            xSemaphoreGive(m_SpriteMutex);
            return true;
        }
    }

    xSemaphoreGive(m_SpriteMutex);
    return false;
}

bool Animation::PopAnimation(){
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (m_animations.size() > 0){
        m_animations.top().ResetIfNeeded();
        m_animations.pop();
        m_breathingEnabled = m_animations.empty() || m_animations.top().m_breathingEnabled;
        m_lastBreathingDraw = 0;
        m_needRedraw = true;
        xSemaphoreGive(m_mutex);
        return true;
    }else{
        xSemaphoreGive(m_mutex);
        return false;
    }
    
}
void Animation::MakeFlip(){
    Devices::Display->flipDma();
    m_needFlip = false;
}

void Animation::SetFFTOverlay(bool set){
    m_fftOverlay = set;
}

void Animation::SetShader(int id, float strenght){
    m_shader = (ShaderType)id;
    m_shaderStrenght = strenght;
}

void Animation::Update(uint32_t dt){

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (m_animations.size() > 0){
        auto &elem = m_animations.top();
        xSemaphoreGive(m_mutex);
        bool canPop = internalUpdate(dt, elem);
        if (canPop){
            PopAnimation();
        }
    }else{
        if (!m_onBlankScreen){
            Devices::Display->clearScreen();
            Devices::Display->drawLine(0          , 0            , CANVAS_WIDTH    , CANVAS_HEIGHT  , Devices::Display->color565(255,255,255));
            Devices::Display->drawLine(0          ,  CANVAS_HEIGHT, CANVAS_WIDTH    , 0             , Devices::Display->color565(255,255,255));

            Devices::Display->drawLine(CANVAS_WIDTH, 0            , CANVAS_WIDTH*2  , CANVAS_HEIGHT  , Devices::Display->color565(255,255,255));
            Devices::Display->drawLine(CANVAS_WIDTH, CANVAS_HEIGHT , CANVAS_WIDTH*2  , 0             , Devices::Display->color565(255,255,255));
            m_needFlip = true;
        }
        xSemaphoreGive(m_mutex);
    }
    if (isManaged() && needFlipScreen()){
        MakeFlip();
    }
}

void Animation::setManaged(bool v){
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_isManaged = v;
    xSemaphoreGive(m_mutex);
}

bool Animation::internalUpdate(uint32_t dt, AnimationSequence &running){
    bool managed = isManaged();
    switch (running.Update(dt, m_interruptPin, managed, m_shader, m_shaderStrenght, m_randomBlinkEnabled)){
    case ANIMATION_FINISHED:
        return 1;
        break;
    case ANIMATION_FRAME_CHANGED:    
        m_lastFace = running.GetFrameId();
        if (managed){
            DrawFrame(m_lastFace);
            m_lastBreathingDraw = millis();
        }
        break;
    case ANIMATION_NEED_FLIP:
        if (managed){
            m_lastFace = running.GetFrameId();
            m_needFlip = true;
        }
        break;
    case ANIMATION_NO_CHANGE:
        const bool redrawForBreathing = isBreathingEnabled() && millis() - m_lastBreathingDraw >= 33;
        if (m_shader != SHADER_NONE || m_fftOverlay || m_forceRedraw || m_needRedraw || redrawForBreathing){
            m_lastFace = running.GetFrameId();
            if (managed){
                DrawFrame(m_lastFace);
                m_lastBreathingDraw = millis();
            }
            m_needRedraw = false;
        }
        break;
    }
    return false;
}


int Animation::getCurrentAnimationStorage(){
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (m_animations.size() > 0){
        auto &elem = m_animations.top();
        int aux = elem.m_storageId;
        xSemaphoreGive(m_mutex);
        return aux;
    }else{
        xSemaphoreGive(m_mutex);
        return -1;
    }   
}

void Animation::SetInterruptAnimation(int duration, std::vector<int> frames){
    

    AnimationSequence newSeq;
    newSeq.m_duration = max(duration,1);
    newSeq.m_frames = PSRAMVector<int>(frames.begin(), frames.end());
    newSeq.m_counter = millis()+duration;
    newSeq.m_frame = 0;
    newSeq.m_repeat = -1;
    newSeq.m_updateMode = 1;
    newSeq.m_breathingEnabled = m_breathingEnabled;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_animations.emplace(newSeq);
    xSemaphoreGive(m_mutex);
}

void Animation::SetModelAnimation(int animationId, int repeatTimes, bool dropAll, int externalStorageId){
    if (dropAll){
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        while (m_animations.size() > 0){
            m_animations.top().ResetIfNeeded();
            m_animations.pop();
        }
        xSemaphoreGive(m_mutex);
    }

    AnimationSequence newSeq;
    newSeq.m_frame = animationId;
    newSeq.m_isNew = true;
    newSeq.m_isModel = true;
    newSeq.m_repeat = repeatTimes;
    newSeq.m_storageId = externalStorageId;
    newSeq.m_breathingEnabled = m_breathingEnabled;

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    newSeq.ResetIfNeeded();
    m_animations.emplace(newSeq);
    xSemaphoreGive(m_mutex);
}

void Animation::SetAnimation(std::vector<int> frames, int duration, int repeatTimes, bool dropAll, int externalStorageId){
    SetAnimationWithFreeze(frames, duration, repeatTimes, dropAll, -1, 0, 0, externalStorageId);
}

void Animation::SetAnimationWithFreeze(std::vector<int> frames, int duration, int repeatTimes, bool dropAll, int freezeFrame, int freezeDurationMin, int freezeDurationMax, int externalStorageId){

    if (dropAll){
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        while (m_animations.size() > 0){
            m_animations.top().ResetIfNeeded();
            m_animations.pop();
        }
        xSemaphoreGive(m_mutex);
    }

    AnimationSequence newSeq;
    newSeq.m_duration = max(duration,1);
    newSeq.m_frames = PSRAMVector<int>(frames.begin(), frames.end());
    newSeq.m_counter = 0;
    newSeq.m_frame = 0;
    newSeq.m_isNew = true;
    newSeq.m_repeat = repeatTimes;
    newSeq.m_storageId = externalStorageId;
    newSeq.m_freezeFrame = freezeFrame;
    newSeq.m_freezeDurationMin = max(freezeDurationMin, 0);
    newSeq.m_freezeDurationMax = max(freezeDurationMax, 0);
    newSeq.m_breathingEnabled = m_breathingEnabled;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    newSeq.ResetIfNeeded();
    m_animations.emplace(newSeq);
    xSemaphoreGive(m_mutex);
}


