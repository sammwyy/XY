#pragma once

#include <tamtypes.h>

#include <string>
#include <vector>

namespace xy {

struct XYSound {
    int sampleRate;
    int channels;
    std::vector<s16> samples;

    XYSound() : sampleRate(44100), channels(2), samples() {}
    bool valid() const { return !samples.empty(); }
};

class XYAudio {
public:
    XYAudio();
    ~XYAudio();

    bool init(int sampleRate = 44100, int channels = 2);
    void shutdown();

    bool loadWav(const char* path, XYSound& out);
    bool loadSnd(const char* path, XYSound& out);
    bool load(const char* path, XYSound& out);  // auto-detect by extension
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
    };

    void mixFrames(int frames);
    s16 sampleAt(const XYSound& sound, int frame, int channel) const;

    bool initialized_;
    int sampleRate_;
    int channels_;
    Voice bgm_;
    static const int kMaxSfx = 8;
    Voice sfx_[kMaxSfx];
    std::vector<s16> mixBuffer_;
};

} // namespace xy

