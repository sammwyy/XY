#pragma once

#include <tamtypes.h>
#include <cstdio>
#include <cstdint>

namespace xy {

struct XYSound {
    int sampleRate;
    int channels;
    s16* samples;
    uint32_t sampleCount;
    
    bool isStreamed;
    char path[128];
    uint32_t dataOffset;
    uint32_t dataSize;

    XYSound();
    ~XYSound();
    bool valid() const { return isStreamed || samples != nullptr; }
};

class XYAudio {
public:
    XYAudio();
    ~XYAudio();

    bool init(int sampleRate = 44100, int channels = 2);
    void shutdown();

    bool load(const char* path, XYSound& out);  
    void playBgm(const XYSound& sound, bool loop = true, float volume = 0.65f);
    void stopBgm();
    void playSfx(const XYSound& sound, float volume = 1.0f);
    void update();

private:
    struct Voice {
        const XYSound* sound;
        int positionFrames;
        float volume;
        bool loop;
        bool active;
        
        FILE* file;
        s16* streamBuffer;
        int bufferOffset;
        int bufferCount;
        s16 lastSample;
    };

    bool loadWav(const char* path, XYSound& out);
    bool loadSnd(const char* path, XYSound& out);

    void mixFrames(int frames);

    bool initialized_;
    int sampleRate_;
    int channels_;
    Voice bgm_;
    static const int kMaxSfx = 8;
    Voice sfx_[kMaxSfx];
    s16* mixBuffer_;

    static const int framesPerUpdate = 256;
    static const int streamBufferSize = 32768; // 64KB
};

} // namespace xy
