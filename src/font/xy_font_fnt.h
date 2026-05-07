#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include "xy_font.h"

namespace xy {

// Internal data structure for a loaded FNT font (dev mode)
// Uses more flexible storage since it's for dev/debug
struct XYFontDataFNT {
    XYFontMetrics metrics;
    int scaleW;
    int scaleH;
    std::vector<XYFontGlyph> glyphs;
    std::vector<XYFontKerning> kernings;
    std::vector<std::string> pages;
};

bool xy_font_fnt_load(const char* path, XYFontDataFNT* out_data);
void xy_font_fnt_unload(XYFontDataFNT* data);
const XYFontGlyph* xy_font_fnt_find_glyph(XYFontDataFNT* data, uint32_t codepoint);
int xy_font_fnt_get_kerning(XYFontDataFNT* data, uint32_t first, uint32_t second);

} // namespace xy
