#include "xy_audio.hpp"

#include <audsrv.h>
#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace xy {

namespace {

u16 read16(const u8* data) {
    return static_cast<u16>(data[0] | (data[1] << 8));
}

u32 read32(const u8* data) {
    return static_cast<u32>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

s16 clamp16(int value) {
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return static_cast<s16>(value);
}

} // namespace

XYAudio::XYAudio()
    : initialized_(false), sampleRate_(44100), channels_(2), bgm_(), sfx_(), mixBuffer_() {
    bgm_.active = false;
    for (int i = 0; i < kMaxSfx; ++i) {
        sfx_[i].active = false;
    }
}

XYAudio::~XYAudio() {
    shutdown();
}

bool XYAudio::init(int sampleRate, int channels) {
    if (initialized_) {
        return true;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;

    SifLoadModule("rom0:LIBSD", 0, nullptr);
    SifLoadModule("host:audsrv.irx", 0, nullptr);

    if (audsrv_init() < 0) {
        return false;
    }
    audsrv_fmt_t format;
    format.freq = sampleRate_;
    format.bits = 16;
    format.channels = channels_;
    if (audsrv_set_format(&format) < 0) {
        return false;
    }
    audsrv_set_volume(MAX_VOLUME);
    mixBuffer_.resize(1024 * channels_);
    initialized_ = true;
    return true;
}

void XYAudio::shutdown() {
    if (!initialized_) {
        return;
    }
    audsrv_stop_audio();
    audsrv_quit();
    initialized_ = false;
}

bool XYAudio::loadWav(const char* path, XYSound& out) {
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }

    u8 header[12];
    if (std::fread(header, 1, sizeof(header), file) != sizeof(header) ||
        std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        std::fclose(file);
        return false;
    }

    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    std::vector<u8> pcmBytes;

    while (!std::feof(file)) {
        u8 chunk[8];
        if (std::fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk)) {
            break;
        }

        u32 size = read32(chunk + 4);
        long next = std::ftell(file) + static_cast<long>((size + 1) & ~1u);

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            std::vector<u8> fmt(size);
            if (std::fread(fmt.data(), 1, size, file) != size) {
                std::fclose(file);
                return false;
            }
            u16 audioFormat = read16(fmt.data());
            channels = read16(fmt.data() + 2);
            sampleRate = static_cast<int>(read32(fmt.data() + 4));
            bitsPerSample = read16(fmt.data() + 14);
            if (audioFormat != 1 || bitsPerSample != 16 || (channels != 1 && channels != 2)) {
                std::fclose(file);
                return false;
            }
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            pcmBytes.resize(size);
            if (std::fread(pcmBytes.data(), 1, size, file) != size) {
                std::fclose(file);
                return false;
            }
        }

        std::fseek(file, next, SEEK_SET);
    }

    std::fclose(file);

    if (sampleRate != sampleRate_ || pcmBytes.empty()) {
        return false;
    }

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.samples.resize(pcmBytes.size() / 2);
    for (unsigned int i = 0; i < out.samples.size(); ++i) {
        out.samples[i] = static_cast<s16>(read16(&pcmBytes[i * 2]));
    }
    return true;
}

bool XYAudio::loadSnd(const char* path, XYSound& out) {
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }

    // Read 64-byte P2SN header
    u8 header[64];
    if (std::fread(header, 1, 64, file) != 64 ||
        std::memcmp(header, "P2SN", 4) != 0) {
        std::fclose(file);
        return false;
    }

    int channels    = read16(header + 6);
    int sampleRate  = static_cast<int>(read32(header + 8));
    u32 dataSize    = read32(header + 24);
    u32 dataOffset  = read32(header + 28);

    if (channels != 1 && channels != 2) {
        std::fclose(file);
        return false;
    }

    if (sampleRate != sampleRate_) {
        std::fclose(file);
        return false;
    }

    std::fseek(file, static_cast<long>(dataOffset), SEEK_SET);
    std::vector<u8> pcmBytes(dataSize);
    if (std::fread(pcmBytes.data(), 1, dataSize, file) != dataSize) {
        std::fclose(file);
        return false;
    }

    std::fclose(file);

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.samples.resize(dataSize / 2);
    for (unsigned int i = 0; i < out.samples.size(); ++i) {
        out.samples[i] = static_cast<s16>(read16(&pcmBytes[i * 2]));
    }
    return true;
}

bool XYAudio::load(const char* path, XYSound& out) {
    std::string p(path);
    std::string ext;
    auto dot = p.find_last_of('.');
    if (dot != std::string::npos) {
        ext = p.substr(dot + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == "snd" || ext == "ps2snd" || ext == "p2s") {
        return loadSnd(path, out);
    }
    return loadWav(path, out);
}

void XYAudio::playBgm(const XYSound& sound, bool loop, float volume) {
    bgm_.sound = &sound;
    bgm_.positionFrames = 0;
    bgm_.volume = volume;
    bgm_.loop = loop;
    bgm_.active = sound.valid();
}

void XYAudio::stopBgm() {
    bgm_.active = false;
}

void XYAudio::playSfx(const XYSound& sound, float volume) {
    if (!sound.valid()) {
        return;
    }

    int slot = -1;
    for (int i = 0; i < kMaxSfx; ++i) {
        if (!sfx_[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
    }

    sfx_[slot].sound = &sound;
    sfx_[slot].positionFrames = 0;
    sfx_[slot].volume = volume;
    sfx_[slot].loop = false;
    sfx_[slot].active = true;
}

void XYAudio::update() {
    if (!initialized_) {
        return;
    }

    const int frames = 512;
    while (audsrv_available() >= frames * channels_ * static_cast<int>(sizeof(s16))) {
        mixFrames(frames);
        audsrv_play_audio(reinterpret_cast<const char*>(mixBuffer_.data()),
                          frames * channels_ * static_cast<int>(sizeof(s16)));
    }
}

void XYAudio::mixFrames(int frames) {
    std::fill(mixBuffer_.begin(), mixBuffer_.begin() + frames * channels_, 0);

    Voice* voices[kMaxSfx + 1];
    voices[0] = &bgm_;
    for (int i = 0; i < kMaxSfx; ++i) {
        voices[i + 1] = &sfx_[i];
    }

    for (int v = 0; v < kMaxSfx + 1; ++v) {
        Voice& voice = *voices[v];
        if (!voice.active || voice.sound == nullptr || !voice.sound->valid()) {
            continue;
        }

        const XYSound& sound = *voice.sound;
        int totalFrames = static_cast<int>(sound.samples.size()) / sound.channels;
        for (int frame = 0; frame < frames; ++frame) {
            if (voice.positionFrames >= totalFrames) {
                if (voice.loop) {
                    voice.positionFrames = 0;
                } else {
                    voice.active = false;
                    break;
                }
            }

            for (int channel = 0; channel < channels_; ++channel) {
                int index = frame * channels_ + channel;
                int mixed = mixBuffer_[index] +
                    static_cast<int>(sampleAt(sound, voice.positionFrames, channel) * voice.volume);
                mixBuffer_[index] = clamp16(mixed);
            }
            ++voice.positionFrames;
        }
    }
}

s16 XYAudio::sampleAt(const XYSound& sound, int frame, int channel) const {
    int sourceChannel = sound.channels == 1 ? 0 : std::min(channel, sound.channels - 1);
    return sound.samples[frame * sound.channels + sourceChannel];
}

} // namespace xy
