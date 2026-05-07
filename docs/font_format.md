# XY Font System Documentation

The XY Font system is a high-performance bitmap font solution for the PlayStation 2. It consists of a custom binary format (`.p2f`) optimized for zero-parsing and fast lookups, and a multi-purpose conversion tool.

## 1. Binary Format Specification (P2F)

| Component | Offset | Alignment | Description |
|-----------|--------|-----------|-------------|
| Header    | 0x00   | 16 bytes  | Magic, Version, Metrics, and Table Offsets |
| Glyph Table| Variable| 16 bytes | Sorted array of glyph metrics and UVs |
| Kerning Table| Variable| 16 bytes | Sorted array of kerning pairs |
| Page Table | Variable| 16 bytes | Atlas metadata (size, name offset) |
| String Table| Variable| 16 bytes | Null-terminated string data for page names |
| Atlas Data | Variable| 16 bytes | Optional raw pixel data |

### Header Flags
- `0x01`: Embedded Atlas (Texture data is inside the P2F file)
- `0x02`: External Atlas (Texture data is in a separate file)
- `0x04`: Has Kerning
- `0x10`: UTF-8 Supported

## 2. Python Tool Usage

### Convert BMFont (AngelCode)
```bash
python tools/ps2fnt.py convert-bmfont assets/fonts/ui.fnt build/fonts/ui.p2f --embed-atlas
```

### Convert TrueType / OpenType
```bash
python tools/ps2fnt.py convert-ttf assets/fonts/main.ttf build/fonts/main.p2f --size 24 --charset ascii --embed-atlas
```

### Convert PNG Grid (Pixel Fonts)
```bash
python tools/ps2fnt.py convert-png assets/fonts/debug8x8.png build/fonts/debug.p2f --cell-width 8 --cell-height 8 --first-codepoint 32
```

## 3. C++ Runtime Usage

### Loading and Unloading
```cpp
XYFontHandle font;
if (XYFont::load("host:build/fonts/main.p2f", &font)) {
    // Font loaded
}

// Later
XYFont::unload(font);
```

### Measuring Text
```cpp
float width = XYFont::measureTextWidth(font, "Hello World!", 1.0f);
```

### Generating Vertices
```cpp
XYTextVertex vertices[512];
XYTextLayoutParams params = {0};
params.x = 100.0f;
params.y = 100.0f;
params.scale = 1.0f;
params.color = 0xFFFFFFFF;

int count = XYFont::layoutText(font, "Score: 1234", params, vertices, 512);

// Submit vertices to your renderer
// vertices contains 'count' vertices (6 per glyph for triangles)
```

## 4. Design Decisions & Optimization

- **Binary Search**: Both glyphs and kernings are stored in sorted arrays. Lookups use binary search, ensuring $O(\log N)$ performance which is highly predictable on PS2.
- **Zero-Copy**: The P2F loader reads the entire file into a single memory block. The runtime tables (glyphs, pages, etc.) point directly into this buffer, avoiding redundant allocations.
- **UTF-8**: The layout engine supports multi-byte UTF-8 sequences.
- **Batched Rendering**: The `layoutText` function emits a flat array of vertices, ready for efficient DMA transfer to the Graphics Synthesizer.

## 5. Limitations

- **No Complex Shaping**: Does not support RTL (Arabic/Hebrew) or complex Indic scripts (no HarfBuzz).
- **Static Sizing**: Atlas is generated offline. No runtime rasterization.
- **Simple Kerning**: GPOS/GSUB tables from modern fonts are not fully supported; only simple (first, second, amount) pairs.
