#include "xy_audio.hpp"

// Timestamp: 2026-05-07 20:40:00

#include <audsrv.h>
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>

namespace xy {

namespace {

u16 read16(const u8* data) {
    return static_cast<u16>(data[0] | (data[1] << 8));
}

u32 read32(const u8* data) {
    return static_cast<u32>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

s16 clamp16(int value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return static_cast<s16>(value);
}

} // namespace

XYSound::XYSound() : sampleRate(44100), channels(2), samples(nullptr), sampleCount(0), 
                    isStreamed(false), dataOffset(0), dataSize(0) {
    std::memset(path, 0, sizeof(path));
}

XYSound::~XYSound() {
    if (samples) std::free(samples);
}

XYAudio::XYAudio()
    : initialized_(false), sampleRate_(44100), channels_(2), bgm_(), sfx_(), mixBuffer_(nullptr) {
    bgm_.active = false;
    bgm_.file = nullptr;
    bgm_.sound = nullptr;
    bgm_.streamBuffer = nullptr;
    for (int i = 0; i < kMaxSfx; ++i) {
        sfx_[i].active = false;
        sfx_[i].file = nullptr;
        sfx_[i].sound = nullptr;
        sfx_[i].streamBuffer = nullptr;
    }
}

XYAudio::~XYAudio() {
    shutdown();
}

bool XYAudio::init(int sampleRate, int channels) {
    if (initialized_) return true;

    sampleRate_ = sampleRate;
    channels_ = channels;

    SifLoadModule("rom0:LIBSD", 0, nullptr);
    SifLoadModule("host:audsrv.irx", 0, nullptr);

    if (audsrv_init() < 0) return false;
    
    audsrv_fmt_t format;
    format.freq = sampleRate_;
    format.bits = 16;
    format.channels = channels_;
    audsrv_set_format(&format);
    audsrv_set_volume(MAX_VOLUME);
    
    mixBuffer_ = (s16*)std::malloc(framesPerUpdate * channels_ * sizeof(s16));
    initialized_ = true;
    return true;
}

void XYAudio::shutdown() {
    if (!initialized_) return;
    stopBgm();
    audsrv_stop_audio();
    audsrv_quit();
    if (mixBuffer_) std::free(mixBuffer_);
    mixBuffer_ = nullptr;
    initialized_ = false;
}

bool XYAudio::loadWav(const char* path, XYSound& out) {
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;

    u8 header[12];
    if (std::fread(header, 1, 12, file) != 12 ||
        std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        std::fclose(file);
        return false;
    }

    int channels = 0;
    int sampleRate = 0;
    u32 dataSize = 0;

    while (!std::feof(file)) {
        u8 chunk[8];
        if (std::fread(chunk, 1, 8, file) != 8) break;
        u32 size = read32(chunk + 4);
        long next = std::ftell(file) + ((size + 1) & ~1u);

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            u8 fmt[16];
            std::fread(fmt, 1, 16, file);
            channels = read16(fmt + 2);
            sampleRate = read32(fmt + 4);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataSize = size;
            out.samples = (s16*)std::malloc(size);
            std::fread(out.samples, 1, size, file);
        }
        std::fseek(file, next, SEEK_SET);
    }
    std::fclose(file);

    if (!out.samples) return false;

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.isStreamed = false;
    out.dataSize = dataSize;
    out.sampleCount = dataSize / (channels * 2);
    std::strncpy(out.path, path, sizeof(out.path) - 1);
    return true;
}

bool XYAudio::loadSnd(const char* path, XYSound& out) {
    FILE* file = std::fopen(path, "rb");
    if (!file) return false;

    u8 header[64];
    if (std::fread(header, 1, 64, file) != 64 || std::memcmp(header, "P2SN", 4) != 0) {
        std::fclose(file);
        return false;
    }

    out.channels = read16(header + 6);
    out.sampleRate = read32(header + 8);
    out.dataSize = read32(header + 24);
    out.dataOffset = read32(header + 28);
    out.sampleCount = out.dataSize / (out.channels * 2);
    std::strncpy(out.path, path, sizeof(out.path) - 1);

    const u32 kMaxResidentBytes = 1024 * 1024; // Increase to 1MB to allow more resident sounds
    
    if (out.dataSize > kMaxResidentBytes) {
        out.isStreamed = true;
        if (out.samples) std::free(out.samples);
        out.samples = nullptr;
        std::fclose(file);
        return true;
    }

    out.isStreamed = false;
    out.samples = (s16*)std::malloc(out.dataSize);
    std::fseek(file, out.dataOffset, SEEK_SET);
    std::fread(out.samples, 1, out.dataSize, file);
    std::fclose(file);
    return true;
}

bool XYAudio::load(const char* path, XYSound& out) {
    const char* ext = std::strrchr(path, '.');
    if (ext && (std::strcmp(ext, ".snd") == 0 || std::strcmp(ext, ".p2s") == 0)) {
        return loadSnd(path, out);
    }
    return loadWav(path, out);
}

void XYAudio::playBgm(const XYSound& sound, bool loop, float volume) {
    stopBgm();
    bgm_.sound = &sound;
    bgm_.positionFrames = 0;
    bgm_.volume = volume;
    bgm_.loop = loop;
    bgm_.active = true;

    if (sound.isStreamed) {
        bgm_.file = std::fopen(sound.path, "rb");
        if (bgm_.file) {
            std::fseek(bgm_.file, sound.dataOffset, SEEK_SET);
            if (!bgm_.streamBuffer) {
                bgm_.streamBuffer = (s16*)std::malloc(streamBufferSize * sizeof(s16));
            }
            bgm_.bufferOffset = 0;
            bgm_.bufferCount = 0;
        } else {
            bgm_.active = false;
        }
    }
}

void XYAudio::stopBgm() {
    bgm_.active = false;
    if (bgm_.file) {
        std::fclose(bgm_.file);
        bgm_.file = nullptr;
    }
}

void XYAudio::playSfx(const XYSound& sound, float volume) {
    int slot = -1;
    for (int i = 0; i < kMaxSfx; ++i) {
        if (!sfx_[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = 0;

    Voice& v = sfx_[slot];
    v.sound = &sound;
    v.positionFrames = 0;
    v.volume = volume;
    v.loop = false;
    v.active = true;
    v.file = nullptr;
}

void XYAudio::update() {
    if (!initialized_ || !mixBuffer_) return;
    
    // Fill the audsrv buffer as much as possible
    int available = audsrv_available();
    while (available >= framesPerUpdate * channels_ * 2) {
        mixFrames(framesPerUpdate);
        audsrv_play_audio((char*)mixBuffer_, framesPerUpdate * channels_ * 2);
        available = audsrv_available();
    }
}

void XYAudio::mixFrames(int frames) {
    std::memset(mixBuffer_, 0, frames * channels_ * sizeof(s16));

    Voice* voices[kMaxSfx + 1];
    voices[0] = &bgm_;
    for (int i = 0; i < kMaxSfx; ++i) voices[i+1] = &sfx_[i];

    for (int i = 0; i < kMaxSfx + 1; ++i) {
        Voice& v = *voices[i];
        if (!v.active || !v.sound) continue;

        const XYSound& s = *v.sound;

        for (int f = 0; f < frames; ++f) {
            if (v.positionFrames >= (int)s.sampleCount) {
                if (v.loop) {
                    v.positionFrames = 0;
                    if (v.file) {
                        std::fseek(v.file, s.dataOffset, SEEK_SET);
                        v.bufferCount = 0;
                        v.bufferOffset = 0;
                    }
                } else {
                    v.active = false;
                    if (v.file) { std::fclose(v.file); v.file = nullptr; }
                    break;
                }
            }

            for (int c = 0; c < channels_; ++c) {
                s16 sample = 0;
                if (s.isStreamed && v.file) {
                    if (s.channels == 1) {
                        if (c == 0) {
                            if (v.bufferOffset >= v.bufferCount) {
                                size_t read = std::fread(v.streamBuffer, sizeof(s16), streamBufferSize, v.file);
                                v.bufferCount = (int)read;
                                v.bufferOffset = 0;
                            }
                            sample = (v.bufferCount > 0) ? v.streamBuffer[v.bufferOffset++] : 0;
                            v.lastSample = sample;
                        } else {
                            sample = v.lastSample;
                        }
                    } else {
                        if (v.bufferOffset >= v.bufferCount) {
                            size_t read = std::fread(v.streamBuffer, sizeof(s16), streamBufferSize, v.file);
                            v.bufferCount = (int)read;
                            v.bufferOffset = 0;
                        }
                        sample = (v.bufferCount > 0) ? v.streamBuffer[v.bufferOffset++] : 0;
                    }
                } else if (s.samples) {
                    int srcChannel = (s.channels == 1) ? 0 : std::min(c, s.channels - 1);
                    sample = s.samples[v.positionFrames * s.channels + srcChannel];
                }

                int mixed = mixBuffer_[f * channels_ + c] + (int)(sample * v.volume);
                mixBuffer_[f * channels_ + c] = clamp16(mixed);
            }
            v.positionFrames++;
        }
    }
}

} // namespace xy
