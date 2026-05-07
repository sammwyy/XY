#pragma once

#include <tamtypes.h>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <cstdint>

namespace xy {

// ---------------------------------------------------------------------------
// PS2 Sound binary format (.snd / .ps2snd / .p2s)
// ---------------------------------------------------------------------------
// Layout:
//   [Ps2SndHeader]  (64 bytes, aligned)
//   [audio data]    (PCM16 LE, 64-byte aligned)
// ---------------------------------------------------------------------------

enum Ps2SndCodec : uint8_t {
    P2SN_CODEC_PCM16 = 1,
    // P2SN_CODEC_VAG_ADPCM = 2,  // future
};

enum Ps2SndFlags : uint32_t {
    P2SN_FLAG_LOOP     = 1 << 0,
    P2SN_FLAG_STEREO   = 1 << 1,
    P2SN_FLAG_STREAMED = 1 << 2,
};

struct Ps2SndHeader {
    char     magic[4];        // "P2SN"
    uint16_t version;         // 1
    uint16_t channels;        // 1 or 2
    uint32_t sample_rate;     // 11025 / 22050 / 44100
    uint32_t sample_count;    // total samples per channel
    uint32_t loop_start;      // sample offset
    uint32_t loop_end;        // sample offset (0 = no loop)
    uint32_t data_size;       // byte size of audio payload
    uint32_t data_offset;     // offset from file start to audio data
    uint32_t flags;           // Ps2SndFlags
    uint8_t  codec;           // Ps2SndCodec
    uint8_t  reserved[27];
} __attribute__((packed));

static_assert(sizeof(Ps2SndHeader) == 64, "Ps2SndHeader must be 64 bytes");

// ---------------------------------------------------------------------------
// Abstract sound resource (analogous to XYImage)
// ---------------------------------------------------------------------------

class XYSoundData {
public:
    virtual ~XYSoundData() {}

    /// Load/decode into EE RAM (samples vector).
    virtual bool load(const std::string& path) = 0;

    /// Release EE RAM.
    virtual void free() = 0;

    virtual int  sampleRate()  const = 0;
    virtual int  channels()    const = 0;
    virtual int  sampleCount() const = 0;    // per channel
    virtual bool looping()     const = 0;
    virtual int  loopStart()   const = 0;
    virtual int  loopEnd()     const = 0;
    virtual bool valid()       const = 0;

    /// Raw interleaved PCM16 samples (channels × sampleCount).
    virtual const s16* samples() const = 0;
    virtual int        samplesSize() const = 0;  // total s16 count
};

// ---------------------------------------------------------------------------
// WAV loader (dev / source format)
// ---------------------------------------------------------------------------

class XYSoundWAV : public XYSoundData {
public:
    XYSoundWAV();
    ~XYSoundWAV() override;

    bool load(const std::string& path) override;
    void free() override;

    int  sampleRate()  const override { return sampleRate_; }
    int  channels()    const override { return channels_; }
    int  sampleCount() const override { return sampleCount_; }
    bool looping()     const override { return false; }
    int  loopStart()   const override { return 0; }
    int  loopEnd()     const override { return 0; }
    bool valid()       const override { return !samples_.empty(); }

    const s16* samples()     const override { return samples_.data(); }
    int        samplesSize() const override { return static_cast<int>(samples_.size()); }

private:
    int sampleRate_;
    int channels_;
    int sampleCount_;
    std::vector<s16> samples_;
};

// ---------------------------------------------------------------------------
// SND loader (PS2 runtime format – P2SN)
// ---------------------------------------------------------------------------

class XYSoundSND : public XYSoundData {
public:
    XYSoundSND();
    ~XYSoundSND() override;

    bool load(const std::string& path) override;
    void free() override;

    int  sampleRate()  const override { return header_.sample_rate; }
    int  channels()    const override { return header_.channels; }
    int  sampleCount() const override { return header_.sample_count; }
    bool looping()     const override { return (header_.flags & P2SN_FLAG_LOOP) != 0; }
    int  loopStart()   const override { return header_.loop_start; }
    int  loopEnd()     const override { return header_.loop_end; }
    bool valid()       const override { return !samples_.empty(); }

    const s16* samples()     const override { return samples_.data(); }
    int        samplesSize() const override { return static_cast<int>(samples_.size()); }

private:
    Ps2SndHeader header_;
    std::vector<s16> samples_;
};

// ---------------------------------------------------------------------------
// Sound Manager (caching / registry, analogous to XYImageManager)
// ---------------------------------------------------------------------------

class XYSoundManager {
public:
    static XYSoundManager& instance();

    std::shared_ptr<XYSoundData> load(const std::string& path);
    void unload(const std::string& path);
    void clear();

private:
    XYSoundManager() {}
    std::map<std::string, std::shared_ptr<XYSoundData>> registry_;
};

// ---------------------------------------------------------------------------
// Convenience wrapper (analogous to XYTexture)
// ---------------------------------------------------------------------------

class XYSoundClip {
public:
    XYSoundClip();
    ~XYSoundClip();

    bool load(const std::string& path);
    void unload();
    void free();

    int  sampleRate()  const;
    int  channels()    const;
    int  sampleCount() const;
    bool looping()     const;
    int  loopStart()   const;
    int  loopEnd()     const;
    bool valid()       const;

    const s16* samples()     const;
    int        samplesSize() const;

    /// Expose underlying data for the mixer.
    XYSoundData* data() const { return sound_.get(); }

private:
    std::shared_ptr<XYSoundData> sound_;
    std::string path_;
};

} // namespace xy
