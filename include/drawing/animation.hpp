#pragma once 
#include "tools/displays.hpp"
#include <vector>
#include <stack>
#include <stdint.h>
#include <FS.h>
#include "config.hpp"
#include "tools/psrammap.hpp"

#include "drawing/rendering/modelhandler.hpp"
#include "drawing/rendering/shader.hpp"
#include "drawing/sprite.hpp"

#include "tools/fft.hpp"
extern FFT g_fft;

enum AnimationFrameAction{
    ANIMATION_NO_CHANGE,
    ANIMATION_FRAME_CHANGED,
    ANIMATION_FINISHED,
    ANIMATION_NEED_FLIP,
};

class FrameRepository;
extern FrameRepository g_frameRepo;

#define MODEL_FRAME_ID_OFFSET 100000


class AnimationSequence{
    public:
        AnimationSequence():m_duration(2500),m_frame(0),m_counter(0),m_repeat(-1),m_updateMode(0),m_storageId(-1),m_freezeFrame(-1),m_freezeDurationMin(0),m_freezeDurationMax(0),m_freezeStartTime(0),m_freezeDuration(0),m_isNew(true),m_isModel(false){}
        PSRAMVector<int> m_frames;
        AnimationFrameAction Update(uint32_t dt, int m_interruptPin, bool isManaged, ShaderType &shdr, float &strenghy);
        inline int GetFrameId();
        void ResetIfNeeded();
        int m_duration;
        int m_frame; //Also used to store the ID of the animation if its a model animation
        int m_counter;
        int m_repeat;
        int m_updateMode;
        int m_storageId;
        int m_freezeFrame;
        int m_freezeDurationMin;
        int m_freezeDurationMax;
        uint32_t m_freezeStartTime;
        uint32_t m_freezeDuration;
        bool m_isNew;
        bool m_isModel;
    private:
        AnimationFrameAction ChangeFrame();
        AnimationFrameAction InterruptFrame(int pinRead);
    
};

class Animation{
    public:
        Animation():m_animations(),m_shader(SHADER_NONE),m_shaderStrenght(1.0f),m_lastFace(0),m_interruptPin(-1),m_colorMode(COLOR_MODE_RGB),m_needFlip(false),m_isManaged(true),m_needRedraw(false),m_onBlankScreen(false),m_copyToFrameBuffer(false),m_fftOverlay(false),m_forceRedraw(false),m_frameDrawDuration(0),m_texture(nullptr),m_frameBuffer(nullptr),m_frameLoadDuration(0),m_cycleDuration(0),m_mutex(xSemaphoreCreateMutex()),m_SpriteMutex(xSemaphoreCreateMutex()){};
        void Allocate();
        void Update(uint32_t dt);

        void SetModelAnimation(int animationId, int repeatTimes, bool dropAll, int externalStorageId=-1);

        void SetAnimation(std::vector<int> frames, int duration, int repeatTimes, bool dropAll, int freezeFrame, int freezeDurationMin, int freezeDurationMax, int externalStorageId=-1);
        void SetInterruptAnimation(int duration, std::vector<int> frames);
        void SetInterruptPin(int pin){
              if (pin > 0){
                pinMode(pin, INPUT);
            }
            m_interruptPin = pin;
        }

        void DrawFrame(int i);
        void LoadFrameAsTexture(int i);
        void DrawCurrentFrame(){
            DrawFrame(m_lastFace);
        }

        void EnableFrameBuffer(bool enable);


        bool PopAnimation();
        void MakeFlip();
        void SetShader(int id, float strenght=1.0f);
        void SetFFTOverlay(bool set);

        uint16_t* GetTexture(){
            return m_texture;
        }
        
        void setColorMode(ColorMode mode){
            m_colorMode = mode;
            m_needRedraw = true;
        };

        bool needFlipScreen(){
            return m_needFlip;
        };
        void setManaged(bool v);
        bool isManaged(){
            return m_isManaged;
        }
        int getCurrentFace(){
            return m_lastFace;
        }

        int getCurrentAnimationStorage();

        float getFps(){
            return 1000000.0f/(float)m_cycleDuration;
        }

        int getAnimationStackSize(){
            return m_animations.size();
        }

        static unsigned char *buffer;

        uint32_t getDrawDuration() { return m_frameDrawDuration;};
        uint32_t getLoadDuration() { return m_frameLoadDuration;};

        void IncludeSpriteInPool(Sprite *s);


        int clearAllOverlaySprites();
        bool setOverlaySprite(Sprite *s);
        bool clearOverlaySprite(Sprite *s);

        void forceRedrawEachFrame(bool f){
            m_forceRedraw = f;
        }

    private:
        void drawFFTOverlay(FlipConfig flipSettings, int16_t frameId);
        inline void drawPixelAt(int16_t &x, int16_t &y, uint16_t &color, uint8_t &r, uint8_t &g, uint8_t &b, int &byteIdOled, FlipConfig &flipSettings);
        std::stack<AnimationSequence> m_animations;
        bool internalUpdate(uint32_t dt, AnimationSequence &seq);
        ShaderType m_shader;
        float m_shaderStrenght;
        int m_lastFace;
        int m_interruptPin;
        ColorMode m_colorMode;
        bool m_needFlip;
        bool m_isManaged;
        bool m_needRedraw;
        bool m_onBlankScreen;
        bool m_copyToFrameBuffer;
        bool m_fftOverlay;
        bool m_forceRedraw;

        uint16_t *m_texture,*m_frameBuffer;
        
        uint64_t m_frameDrawDuration;
        uint64_t m_frameLoadDuration;
        uint64_t m_cycleDuration;
        SemaphoreHandle_t m_mutex;     
        SemaphoreHandle_t m_SpriteMutex;     
        
        std::vector<Sprite*> m_overlaySprites;
        std::vector<int> m_spritesInScene;
};


extern Animation g_animation;
