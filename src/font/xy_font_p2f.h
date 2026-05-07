#pragma once

#include <stdint.h>
#include "xy_font.h"

namespace xy {

#pragma pack(push, 1)

enum XYP2FFlags {
    XY_P2F_FLAG_EMBEDDED_ATLAS = 1 << 0,
    XY_P2F_FLAG_EXTERNAL_ATLAS = 1 << 1,
    XY_P2F_FLAG_HAS_KERNING    = 1 << 2,
    XY_P2F_FLAG_MONOSPACE      = 1 << 3,
    XY_P2F_FLAG_UTF8           = 1 << 4,
};

struct XYP2FHeader {
    char magic[4];              // "P2FN"
    uint16_t version;           // 1
    uint16_t header_size;

    uint16_t glyph_count;
    uint16_t kerning_count;
    uint16_t page_count;
    uint16_t flags;

    int16_t line_height;
    int16_t base;
    int16_t ascent;
    int16_t descent;

    uint16_t atlas_width;
    uint16_t atlas_height;

    uint32_t glyphs_offset;
    uint32_t kernings_offset;
    uint32_t pages_offset;
    uint32_t strings_offset;

    uint32_t embedded_atlas_offset;
    uint32_t embedded_atlas_size;

    uint32_t total_size;
    uint32_t reserved[8];
};

struct XYP2FPage {
    uint16_t page_id;
    uint16_t width;
    uint16_t height;
    uint16_t reserved;

    uint32_t texture_offset;
    uint32_t texture_size;

    uint32_t name_offset;       // offset into string table if external
    uint32_t flags;
};

struct XYP2FGlyph {
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

struct XYP2FKerning {
    uint32_t first;
    uint32_t second;
    int16_t amount;
};

#pragma pack(pop)

// Internal data structure for a loaded P2F font
struct XYFontDataP2F {
    XYP2FHeader header;
    XYP2FGlyph* glyphs;
    XYP2FKerning* kernings;
    XYP2FPage* pages;
    const char* string_table;
    uint8_t* buffer; // Full file buffer
};

bool xy_font_p2f_load(const char* path, XYFontDataP2F* out_data);
void xy_font_p2f_unload(XYFontDataP2F* data);
const XYFontGlyph* xy_font_p2f_find_glyph(XYFontDataP2F* data, uint32_t codepoint);
int xy_font_p2f_get_kerning(XYFontDataP2F* data, uint32_t first, uint32_t second);

} // namespace xy
