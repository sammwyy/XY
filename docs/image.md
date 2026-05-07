# Image System

Xy's image pipeline supports loading textures from multiple formats and uploading them to GS VRAM.

## Formats

| Extension | Type | Description |
|---|---|---|
| `.png` | Source / dev | Standard PNG via libpng. Loaded and uploaded at runtime. |
| `.jpg` `.jpeg` | Source / dev | JPEG via libjpeg. Loaded and uploaded at runtime. |
| `.ps2tex` `.p2t` `.tex` | PS2 runtime | Custom P2TX binary format. Pre-converted, GS-ready pixel data. |

For production use `.ps2tex` — no runtime decoding, pre-swizzled CLUT, DMA-aligned.

## Pipeline

```
.png / .jpg (source)
  ↓ tools/ps2tex.py
.ps2tex (P2TX binary)
  ↓ PS2 runtime
EE RAM → DMA → GS VRAM
```

## Converter Tool

```bash
# Default (PSMT8 indexed, 256 colors)
python tools/ps2tex.py sprite.png sprite.ps2tex

# Explicit format
python tools/ps2tex.py sprite.png sprite.ps2tex --format psmt8
python tools/ps2tex.py sprite.png sprite.ps2tex --format psmt4
python tools/ps2tex.py sprite.png sprite.ps2tex --format ct16
python tools/ps2tex.py sprite.png sprite.ps2tex --format ct32

# With offline swizzling
python tools/ps2tex.py sprite.png sprite.ps2tex --format psmt8 --swizzle
```

### Formats

| Format | BPP | CLUT | Best for |
|---|---|---|---|
| `psmt8` | 8 | 256 colors, CT32 | Sprites, UI (default) |
| `psmt4` | 4 | 16 colors, CT32 | Simple graphics, fonts |
| `ct16` | 16 | None | Photos, 1-bit alpha |
| `ct32` | 32 | None | Full quality RGBA |

### Size Reference

```
128×128 PSMT8  ≈  17 KB (+ 1 KB CLUT)
128×128 CT16   ≈  32 KB
128×128 CT32   ≈  64 KB
256×256 PSMT8  ≈  66 KB (+ 1 KB CLUT)
```

## P2TX Format

40-byte header followed by DMA-aligned pixel data and optional CLUT.

```c
struct Ps2TexHeader {
    char     magic[4];        // "P2TX"
    uint16_t version;         // 1
    uint16_t width;
    uint16_t height;
    uint8_t  psm;             // GS pixel storage mode
    uint8_t  has_clut;
    uint8_t  clut_psm;
    uint8_t  mip_count;
    uint32_t data_size;
    uint32_t clut_size;
    uint32_t data_offset;
    uint32_t clut_offset;
    uint32_t flags;
    uint8_t  reserved[6];
}; // 40 bytes, packed
```

### Flags

| Bit | Name | Description |
|---|---|---|
| 0 | `SWIZZLED` | Pixel data is GS-swizzled |
| 1 | `CLUT_ROTATED` | CLUT has PS2 CSM1 rotation applied |
| 2 | `HAS_ALPHA` | Image has transparency |

### Layout

```
[Ps2TexHeader]     40 bytes
[pixel data]       128-byte aligned
[CLUT data]        128-byte aligned (if indexed)
```

## Engine API

### Quick (XYTexture)

```cpp
#include "image/xy_image.hpp"

xy::XYTexture tex;
tex.load(gs, "host:assets/sprite.ps2tex");

// Use
graphics().drawTexture(tex, x, y);
graphics().drawTexture(tex, x, y, w, h);

// Cleanup
tex.unload(gs);
tex.free();
```

### Image Manager (caching)

```cpp
// Loads and caches automatically — same path returns same resource
auto img = xy::XYImageManager::instance().load(gs, "host:assets/sprite.ps2tex");

// Dimensions
int w = img->width();
int h = img->height();

// Access raw GSTEXTURE for custom rendering
GSTEXTURE* raw = img->raw();
```

### Format Auto-Detection

The manager picks the loader by extension:

| Extension | Loader |
|---|---|
| `.png` | `XYImagePNG` — libpng decode + VRAM upload |
| `.jpg` `.jpeg` | `XYImageJPG` — libjpeg decode + VRAM upload |
| `.ps2tex` `.p2t` `.tex` | `XYImageP2T` — direct binary load + VRAM upload |

### Memory Flow

```
                    loadEE()           loadGS()
Source file  ──→  EE RAM (decode)  ──→  GS VRAM
                                        ↓
                                   freeEE() (optional)
                                   
                    unloadGS()
                  ←── VRAM freed
```

For `.ps2tex` files, `loadEE()` only stores the path. The actual file read happens in `loadGS()`, which reads the header + pixel data, allocates VRAM, uploads via DMA, then frees the EE buffer immediately.

## Guidelines

| Use case | Recommendation |
|---|---|
| Sprites / UI | PSMT8 (256 colors, small, fast) |
| Simple icons | PSMT4 (16 colors, tiny) |
| Backgrounds | CT16 or PSMT8 depending on quality needs |
| Photos / gradients | CT32 (full quality, large) |
| Dev / prototyping | `.png` / `.jpg` directly |

## VRAM Budget

PS2 GS has 4 MB of VRAM shared between framebuffers, textures, and CLUTs.

```
Typical dual-buffer 640×448 CT16: ~1.1 MB for framebuffers
Remaining for textures: ~2.9 MB
```

Use `XYVramAllocator::usedBytes()` and `XYVramAllocator::totalBytes()` to monitor VRAM usage.
