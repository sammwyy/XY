#include "xy_sound.hpp"
#include <cstdio>
#include <cstring>

namespace xy {

XYSoundSND::XYSoundSND() {
    std::memset(&header_, 0, sizeof(header_));
}

XYSoundSND::~XYSoundSND() {
    free();
}

bool XYSoundSND::load(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::printf("[P2SN] Cannot open: %s\n", path.c_str());
        return false;
    }

    if (std::fread(&header_, 1, sizeof(header_), fp) != sizeof(header_)) {
        std::printf("[P2SN] Error reading header: %s\n", path.c_str());
        std::fclose(fp);
        return false;
    }

    if (std::strncmp(header_.magic, "P2SN", 4) != 0) {
        std::printf("[P2SN] Invalid magic in: %s\n", path.c_str());
        std::fclose(fp);
        return false;
    }

    if (header_.codec != P2SN_CODEC_PCM16) {
        std::printf("[P2SN] Unsupported codec %d: %s\n", header_.codec, path.c_str());
        std::fclose(fp);
        return false;
    }

    if (header_.channels != 1 && header_.channels != 2) {
        std::printf("[P2SN] Bad channels %d: %s\n", header_.channels, path.c_str());
        std::fclose(fp);
        return false;
    }

    std::fseek(fp, header_.data_offset, SEEK_SET);

    int totalSamples = static_cast<int>(header_.data_size / 2);
    samples_.resize(totalSamples);

    size_t bytesRead = std::fread(samples_.data(), 1, header_.data_size, fp);
    std::fclose(fp);

    if (bytesRead != header_.data_size) {
        int actual = static_cast<int>(bytesRead / 2);
        samples_.resize(actual);
    }

    std::printf("[P2SN] Loaded %s (ch=%d, rate=%lu, samples=%lu, flags=0x%02lX)\n",
                path.c_str(), header_.channels,
                (unsigned long)header_.sample_rate,
                (unsigned long)header_.sample_count,
                (unsigned long)header_.flags);

    return !samples_.empty();
}

void XYSoundSND::free() {
    samples_.clear();
    samples_.shrink_to_fit();
    std::memset(&header_, 0, sizeof(header_));
}

} // namespace xy
