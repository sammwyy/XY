#pragma once

#include <stdint.h>
#include <stdbool.h>

struct gsGlobal;
struct gsTexture;

namespace xy {
class XYTexture;

/**
 * XY Font System
 * Optimized for PS2 runtime usage.
 */

typedef uint16_t XYFontHandle;
#define XY_FONT_INVALID_HANDLE 0xFFFF

struct XYFontGlyph {
    uint32_t codepoint;
    uint16_t page;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t x_offset;
    int16_t y_offset;
    int16_t x_advance;
    uint32_t reserved;
    float u0;
    float v0;
    float u1;
    float v1;
};

struct XYFontKerning {
    uint32_t first;
    uint32_t second;
    int16_t amount;
};

struct XYFontMetrics {
    int16_t line_height;
    int16_t base;
    int16_t ascent;
    int16_t descent;
};

struct XYTextVertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
    uint16_t page;
};

struct XYTextLayoutParams {
    float x;
    float y;
    float scale;
    uint32_t color;
    float spacing;
    float line_spacing;
    // Future: alignment, word wrap
};

class XYFont {
public:
    // Initialize font system (allocate internal pools if needed)
    static void init();
    static void shutdown();

    // Load a font (detects format by extension or magic)
    static bool load(const char* path, XYFontHandle* out_font);
    static bool loadGrid(const char* path, int char_width, int char_height, XYFontHandle* out_font);
    static void unload(XYFontHandle font);

    // Manual texture management
    static bool addPage(XYFontHandle font, struct gsTexture* texture);
    static bool addPage(XYFontHandle font, XYTexture* texture);

    // Metadata
    static const XYFontGlyph* findGlyph(XYFontHandle font, uint32_t codepoint);
    static int getKerning(XYFontHandle font, uint32_t first, uint32_t second);
    static XYFontMetrics getMetrics(XYFontHandle font);
    static int getPageCount(XYFontHandle font);
    static const char* getPagePath(XYFontHandle font, int page_index);

    // Texture management
    static bool loadTextures(XYFontHandle font, struct gsGlobal* gs);
    static void unloadTextures(XYFontHandle font, struct gsGlobal* gs);
    static struct gsTexture* getPageTexture(XYFontHandle font, int page_index);

    // Measuring
    static float measureTextWidth(XYFontHandle font, const char* utf8, float scale = 1.0f);
    static void measureText(XYFontHandle font, const char* utf8, float scale, float* out_width, float* out_height);

    // Layout
    // Returns number of vertices generated
    static int layoutText(
        XYFontHandle font,
        const char* utf8,
        XYTextLayoutParams params,
        XYTextVertex* out_vertices,
        int max_vertices
    );

    // UTF-8 Helper
    static bool nextCodepoint(const char** cursor, uint32_t* out_codepoint);
};

} // namespace xy
