# Audio System

Xy's audio pipeline has two layers:

1. **Mixer** (`xy_audio`) — low-level voice mixer using audsrv/SPU2.
2. **Sound module** (`sound/`) — format loaders and resource management.

## Formats

| Extension | Type | Description |
|---|---|---|
| `.wav` | Source / dev | Standard PCM16 WAV. Loaded at runtime via RIFF parser. |
| `.snd` `.ps2snd` `.p2s` | PS2 runtime | Custom P2SN binary format. 64-byte header + aligned PCM16 data. |

For production use `.snd` — smaller overhead, no chunk parsing, 64-byte aligned for DMA.

## Pipeline

```
.wav (source)
  ↓ tools/ps2snd.py
.snd (P2SN binary)
  ↓ PS2 runtime
XYAudio mixer → audsrv → SPU2
```

## Converter Tool

```bash
# Basic conversion
python tools/ps2snd.py input.wav output.snd

# With loop points (sample offsets, per channel)
python tools/ps2snd.py bgm.wav bgm.snd --loop-start 10000 --loop-end 250000

# Force mono downmix
python tools/ps2snd.py explosion.wav explosion.snd --mono

# Resample to 22050 Hz (halves file size)
python tools/ps2snd.py ambient.wav ambient.snd --resample 22050

# Combine options
python tools/ps2snd.py bgm.wav bgm.snd --mono --resample 22050 --loop-start 0 --loop-end 100000
```

### Options

| Flag | Description |
|---|---|
| `--loop-start N` | Loop start sample (per channel) |
| `--loop-end N` | Loop end sample (0 = no loop) |
| `--mono` | Downmix stereo to mono |
| `--resample RATE` | Resample to target rate (e.g. 22050) |

## P2SN Format

64-byte header followed by 64-byte-aligned PCM16 audio data.

```c
struct Ps2SndHeader {
    char     magic[4];        // "P2SN"
    uint16_t version;         // 1
    uint16_t channels;        // 1 or 2
    uint32_t sample_rate;     // 11025 / 22050 / 44100
    uint32_t sample_count;    // total samples per channel
    uint32_t loop_start;      // sample offset
    uint32_t loop_end;        // sample offset (0 = no loop)
    uint32_t data_size;       // byte size of audio payload
    uint32_t data_offset;     // offset from file start (always 64)
    uint32_t flags;           // see below
    uint8_t  codec;           // 1 = PCM16
    uint8_t  reserved[27];
}; // 64 bytes, packed
```

### Flags

| Bit | Name | Description |
|---|---|---|
| 0 | `LOOP` | Audio has loop points |
| 1 | `STEREO` | 2-channel interleaved |
| 2 | `STREAMED` | Reserved for future streaming |

### Codecs

| Value | Name | Status |
|---|---|---|
| 1 | PCM16 | ✅ Supported |
| 2 | VAG/ADPCM | 🔜 Planned |

## Size Reference

```
1s mono   44100 Hz PCM16 ≈  88 KB
1s stereo 44100 Hz PCM16 ≈ 176 KB
1s mono   22050 Hz PCM16 ≈  44 KB
1s mono   44100 Hz ADPCM ≈  24 KB (future)
```

## Engine API

### Quick (via XYAudio mixer)

```cpp
// Auto-detect format by extension
xy::XYSound sfx;
audio().load("host:assets/jump.snd", sfx);
audio().playSfx(sfx);

// Explicit format
audio().loadWav("host:assets/jump.wav", sfx);
audio().loadSnd("host:assets/jump.snd", sfx);

// BGM with loop
xy::XYSound bgm;
audio().load("host:assets/bgm.snd", bgm);
audio().playBgm(bgm, true, 0.45f);
```

### Sound Module (resource management)

```cpp
#include "sound/xy_sound.hpp"

// Managed loading with caching
xy::XYSoundClip clip;
clip.load("host:assets/jump.snd");

// Access raw PCM data
const s16* data = clip.samples();
int count = clip.samplesSize();
int rate = clip.sampleRate();

// Or use the manager directly
auto snd = xy::XYSoundManager::instance().load("host:assets/jump.snd");
```

## Guidelines

| Use case | Recommendation |
|---|---|
| Short SFX | `.snd` PCM16, mono, 22050 Hz |
| BGM loops | `.snd` PCM16, stereo, 44100 Hz with loop points |
| Ambient/long | Future: streaming `.snd` with chunks |
| Debug/dev | `.wav` directly (no conversion needed) |

## Roadmap

1. ✅ `.snd` PCM16 mono/stereo
2. ✅ Mixer with voices (BGM + 8 SFX)
3. 🔜 `.ps2bank` sound bank format
4. 🔜 Streaming by chunks for music
5. 🔜 VAG/ADPCM encoder + codec
6. 🔜 Sample-accurate loops
7. 🔜 Priority / voice stealing
8. 🔜 Spatial audio
9. 🔜 SPU2 reverb/effects
