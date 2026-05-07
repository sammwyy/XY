#include "xy_sound.hpp"
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

} // namespace

XYSoundWAV::XYSoundWAV()
    : sampleRate_(0), channels_(0), sampleCount_(0) {}

XYSoundWAV::~XYSoundWAV() {
    free();
}

bool XYSoundWAV::load(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::printf("[WAV] Cannot open: %s\n", path.c_str());
        return false;
    }

    // Read RIFF header
    u8 riff[12];
    if (std::fread(riff, 1, sizeof(riff), fp) != sizeof(riff) ||
        std::memcmp(riff, "RIFF", 4) != 0 ||
        std::memcmp(riff + 8, "WAVE", 4) != 0) {
        std::printf("[WAV] Invalid RIFF/WAVE header: %s\n", path.c_str());
        std::fclose(fp);
        return false;
    }

    int channels = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    std::vector<u8> pcmBytes;

    // Parse chunks
    while (!std::feof(fp)) {
        u8 chunk[8];
        if (std::fread(chunk, 1, sizeof(chunk), fp) != sizeof(chunk)) {
            break;
        }

        u32 chunkSize = read32(chunk + 4);
        long nextChunk = std::ftell(fp) + static_cast<long>((chunkSize + 1) & ~1u);

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            std::vector<u8> fmt(chunkSize);
            if (std::fread(fmt.data(), 1, chunkSize, fp) != chunkSize) {
                std::fclose(fp);
                return false;
            }

            u16 audioFormat = read16(fmt.data());
            channels = read16(fmt.data() + 2);
            sampleRate = static_cast<int>(read32(fmt.data() + 4));
            bitsPerSample = read16(fmt.data() + 14);

            // Only PCM16, mono or stereo
            if (audioFormat != 1 || bitsPerSample != 16 ||
                (channels != 1 && channels != 2)) {
                std::printf("[WAV] Unsupported format (fmt=%d, bits=%d, ch=%d): %s\n",
                            audioFormat, bitsPerSample, channels, path.c_str());
                std::fclose(fp);
                return false;
            }
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            pcmBytes.resize(chunkSize);
            if (std::fread(pcmBytes.data(), 1, chunkSize, fp) != chunkSize) {
                std::fclose(fp);
                return false;
            }
        }

        std::fseek(fp, nextChunk, SEEK_SET);
    }

    std::fclose(fp);

    if (sampleRate == 0 || pcmBytes.empty()) {
        std::printf("[WAV] No valid audio data in: %s\n", path.c_str());
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;

    // Convert raw bytes to s16 samples
    int totalSamples = static_cast<int>(pcmBytes.size() / 2);
    samples_.resize(totalSamples);
    for (int i = 0; i < totalSamples; ++i) {
        samples_[i] = static_cast<s16>(read16(&pcmBytes[i * 2]));
    }

    sampleCount_ = totalSamples / channels_;

    std::printf("[WAV] Loaded %s (ch=%d, rate=%d, samples=%d)\n",
                path.c_str(), channels_, sampleRate_, sampleCount_);
    return true;
}

void XYSoundWAV::free() {
    samples_.clear();
    samples_.shrink_to_fit();
    sampleRate_ = 0;
    channels_ = 0;
    sampleCount_ = 0;
}

} // namespace xy
