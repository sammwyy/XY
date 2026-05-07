#include "xy_font.h"
#include "xy_font_p2f.h"
#include "xy_font_fnt.h"
#include "image/xy_image.hpp"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

namespace xy {

#define MAX_FONTS 16

enum FontType {
    FONT_TYPE_NONE,
    FONT_TYPE_P2F,
    FONT_TYPE_FNT
};

struct FontSlot {
    FontType type;
    XYFontDataP2F p2f;
    XYFontDataFNT fnt;
    XYTexture* textures;
    int texture_count;
    char base_dir[256];
};

static FontSlot s_font_slots[MAX_FONTS];

void XYFont::init() {
    for (int i = 0; i < MAX_FONTS; i++) {
        s_font_slots[i].type = FONT_TYPE_NONE;
        s_font_slots[i].textures = nullptr;
        s_font_slots[i].texture_count = 0;
        s_font_slots[i].base_dir[0] = '\0';
    }
}

void XYFont::shutdown() {
    for (int i = 0; i < MAX_FONTS; i++) {
        unload((XYFontHandle)i);
    }
}

bool XYFont::load(const char* path, XYFontHandle* out_font) {
    int slot_idx = -1;
    for (int i = 0; i < MAX_FONTS; i++) {
        if (s_font_slots[i].type == FONT_TYPE_NONE) {
            slot_idx = i;
            break;
        }
    }

    if (slot_idx == -1) return false;

    // Extract base directory
    std::string path_str(path);
    size_t last_slash = path_str.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        strncpy(s_font_slots[slot_idx].base_dir, path_str.substr(0, last_slash + 1).c_str(), 255);
    } else {
        s_font_slots[slot_idx].base_dir[0] = '\0';
    }

    // Detect format
    const char* ext = strrchr(path, '.');
    if (ext && (strcmp(ext, ".p2f") == 0 || strcmp(ext, ".ps2fnt") == 0)) {
        if (xy_font_p2f_load(path, &s_font_slots[slot_idx].p2f)) {
            s_font_slots[slot_idx].type = FONT_TYPE_P2F;
            *out_font = (XYFontHandle)slot_idx;
            return true;
        }
    } else if (ext && (strcmp(ext, ".fnt") == 0 || strcmp(ext, ".font") == 0)) {
        if (xy_font_fnt_load(path, &s_font_slots[slot_idx].fnt)) {
            s_font_slots[slot_idx].type = FONT_TYPE_FNT;
            *out_font = (XYFontHandle)slot_idx;
            return true;
        }
    } else if (ext && (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0 || strcmp(ext, ".p2t") == 0)) {
        return loadGrid(path, 16, 16, out_font); // Default guess for grid
    }

    return false;
}

bool XYFont::loadGrid(const char* path, int char_width, int char_height, XYFontHandle* out_font) {
    int slot_idx = -1;
    for (int i = 0; i < MAX_FONTS; i++) {
        if (s_font_slots[i].type == FONT_TYPE_NONE) {
            slot_idx = i;
            break;
        }
    }

    if (slot_idx == -1) return false;

    // Grid font is essentially a simplified FNT
    s_font_slots[slot_idx].type = FONT_TYPE_FNT;
    XYFontDataFNT& fnt = s_font_slots[slot_idx].fnt;
    fnt.metrics.line_height = char_height;
    fnt.metrics.base = char_height;
    fnt.metrics.ascent = char_height;
    fnt.metrics.descent = 0;
    fnt.pages.push_back(path);

    // Texture scale is required for normalized UV calculations in FNT fonts.
    // Defaulting to 1.0 until the texture is loaded and dimensions are known.
    fnt.scaleW = 1;
    fnt.scaleH = 1;

    // Create 96 ASCII glyphs (32-127)
    for (int i = 0; i < 96; i++) {
        XYFontGlyph g;
        g.codepoint = 32 + i;
        g.page = 0;
        g.width = char_width;
        g.height = char_height;
        g.x_offset = 0;
        g.y_offset = 0;
        g.x_advance = char_width;
        
        // We'll calculate x/y assuming 16 chars per row
        int row = i / 16;
        int col = i % 16;
        g.x = col * char_width;
        g.y = row * char_height;
        
        // UVs will be pixels for now, XYGraphics multiplies them by width
        // Wait, FNT UVs are normalized. So we need scaleW/H to be the image size.
        // For now let's set UVs = pixels and scaleW = 1.
        g.u0 = (float)g.x;
        g.v0 = (float)g.y;
        g.u1 = (float)(g.x + g.width);
        g.v1 = (float)(g.y + g.height);

        fnt.glyphs.push_back(g);
    }

    *out_font = (XYFontHandle)slot_idx;
    return true;
}

void XYFont::unload(XYFontHandle font) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return;

    if (s_font_slots[font].type == FONT_TYPE_P2F) {
        xy_font_p2f_unload(&s_font_slots[font].p2f);
    } else if (s_font_slots[font].type == FONT_TYPE_FNT) {
        xy_font_fnt_unload(&s_font_slots[font].fnt);
    }

    if (s_font_slots[font].textures) {
        delete[] s_font_slots[font].textures;
        s_font_slots[font].textures = nullptr;
    }
    s_font_slots[font].texture_count = 0;

    s_font_slots[font].type = FONT_TYPE_NONE;
}

bool XYFont::addPage(XYFontHandle font, struct gsTexture* texture) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return false;
    
    // XYTexture is a wrapper around shared_ptr/raw GSTEXTURE.
    // Currently, this override is a placeholder for manual texture registration.
    return false;
}

bool XYFont::addPage(XYFontHandle font, XYTexture* texture) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return false;

    if (!s_font_slots[font].textures) {
        s_font_slots[font].textures = new XYTexture[1];
        s_font_slots[font].texture_count = 1;
    }
    
    // This is tricky because XYTexture uses shared_ptr.
    // We'll just copy it.
    s_font_slots[font].textures[0] = *texture;
    return true;
}

const XYFontGlyph* XYFont::findGlyph(XYFontHandle font, uint32_t codepoint) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return nullptr;

    if (s_font_slots[font].type == FONT_TYPE_P2F) {
        return xy_font_p2f_find_glyph(&s_font_slots[font].p2f, codepoint);
    } else {
        return xy_font_fnt_find_glyph(&s_font_slots[font].fnt, codepoint);
    }
}

int XYFont::getKerning(XYFontHandle font, uint32_t first, uint32_t second) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return 0;

    if (s_font_slots[font].type == FONT_TYPE_P2F) {
        return xy_font_p2f_get_kerning(&s_font_slots[font].p2f, first, second);
    } else {
        return xy_font_fnt_get_kerning(&s_font_slots[font].fnt, first, second);
    }
}

XYFontMetrics XYFont::getMetrics(XYFontHandle font) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return {0};

    if (s_font_slots[font].type == FONT_TYPE_P2F) {
        XYP2FHeader& h = s_font_slots[font].p2f.header;
        return {h.line_height, h.base, h.ascent, h.descent};
    } else {
        return s_font_slots[font].fnt.metrics;
    }
}

int XYFont::getPageCount(XYFontHandle font) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return 0;
    if (s_font_slots[font].type == FONT_TYPE_P2F) return s_font_slots[font].p2f.header.page_count;
    return (int)s_font_slots[font].fnt.pages.size();
}

const char* XYFont::getPagePath(XYFontHandle font, int page_index) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return nullptr;

    if (s_font_slots[font].type == FONT_TYPE_P2F) {
        XYFontDataP2F& d = s_font_slots[font].p2f;
        if (page_index < 0 || page_index >= d.header.page_count) return nullptr;
        return d.string_table + d.pages[page_index].name_offset;
    } else {
        XYFontDataFNT& d = s_font_slots[font].fnt;
        if (page_index < 0 || page_index >= (int)d.pages.size()) return nullptr;
        return d.pages[page_index].c_str();
    }
}

bool XYFont::nextCodepoint(const char** cursor, uint32_t* out_codepoint) {
    const uint8_t* p = (const uint8_t*)*cursor;
    if (*p == 0) return false;

    if ((*p & 0x80) == 0) {
        *out_codepoint = *p;
        *cursor += 1;
    } else if ((*p & 0xE0) == 0xC0) {
        *out_codepoint = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
        *cursor += 2;
    } else if ((*p & 0xF0) == 0xE0) {
        *out_codepoint = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *cursor += 3;
    } else if ((*p & 0xF8) == 0xF0) {
        *out_codepoint = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        *cursor += 4;
    } else {
        *out_codepoint = '?';
        *cursor += 1;
    }
    return true;
}

float XYFont::measureTextWidth(XYFontHandle font, const char* utf8, float scale) {
    float w = 0, h = 0;
    measureText(font, utf8, scale, &w, &h);
    return w;
}

void XYFont::measureText(XYFontHandle font, const char* utf8, float scale, float* out_width, float* out_height) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) {
        *out_width = 0; *out_height = 0;
        return;
    }

    XYFontMetrics metrics = getMetrics(font);
    float x = 0;
    float max_x = 0;
    float y = metrics.line_height * scale;
    uint32_t prev_cp = 0;

    const char* cursor = utf8;
    uint32_t cp;
    while (nextCodepoint(&cursor, &cp)) {
        if (cp == '\n') {
            if (x > max_x) max_x = x;
            x = 0;
            y += metrics.line_height * scale;
            prev_cp = 0;
            continue;
        }

        const XYFontGlyph* glyph = findGlyph(font, cp);
        if (!glyph) glyph = findGlyph(font, '?');
        if (!glyph) continue;

        x += getKerning(font, prev_cp, cp) * scale;
        x += glyph->x_advance * scale;
        prev_cp = cp;
    }

    if (x > max_x) max_x = x;
    *out_width = max_x;
    *out_height = y;
}

int XYFont::layoutText(
    XYFontHandle font,
    const char* utf8,
    XYTextLayoutParams params,
    XYTextVertex* out_vertices,
    int max_vertices
) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return 0;

    XYFontMetrics metrics = getMetrics(font);
    float x = params.x;
    float y = params.y;
    uint32_t prev_cp = 0;
    int v_count = 0;

    const char* cursor = utf8;
    uint32_t cp;
    while (nextCodepoint(&cursor, &cp)) {
        if (v_count + 6 > max_vertices) break;

        if (cp == '\n') {
            x = params.x;
            y += (metrics.line_height + params.line_spacing) * params.scale;
            prev_cp = 0;
            continue;
        }

        const XYFontGlyph* glyph = findGlyph(font, cp);
        if (!glyph) glyph = findGlyph(font, '?');
        if (!glyph) continue;

        x += (getKerning(font, prev_cp, cp) + params.spacing) * params.scale;

        float gx = x + glyph->x_offset * params.scale;
        float gy = y + glyph->y_offset * params.scale;
        float gw = glyph->width * params.scale;
        float gh = glyph->height * params.scale;

        // Triangle 1
        out_vertices[v_count++] = { gx,      gy,      glyph->u0, glyph->v0, params.color, glyph->page };
        out_vertices[v_count++] = { gx + gw, gy,      glyph->u1, glyph->v0, params.color, glyph->page };
        out_vertices[v_count++] = { gx,      gy + gh, glyph->u0, glyph->v1, params.color, glyph->page };

        // Triangle 2
        out_vertices[v_count++] = { gx + gw, gy,      glyph->u1, glyph->v0, params.color, glyph->page };
        out_vertices[v_count++] = { gx + gw, gy + gh, glyph->u1, glyph->v1, params.color, glyph->page };
        out_vertices[v_count++] = { gx,      gy + gh, glyph->u0, glyph->v1, params.color, glyph->page };

        x += glyph->x_advance * params.scale;
        prev_cp = cp;
    }

    return v_count;
}

bool XYFont::loadTextures(XYFontHandle font, GSGLOBAL* gs) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return false;
    
    int count = getPageCount(font);
    if (count <= 0) return true;

    s_font_slots[font].textures = new XYTexture[count];
    s_font_slots[font].texture_count = count;

    for (int i = 0; i < count; i++) {
        const char* page_path = getPagePath(font, i);
        std::string full_path = std::string(s_font_slots[font].base_dir) + page_path;
        std::printf("[XYFont] Loading page %d: %s\n", i, full_path.c_str());
        if (!s_font_slots[font].textures[i].load(gs, full_path)) {
            std::printf("[XYFont] Failed to load page %d\n", i);
        }
    }

    return true;
}

void XYFont::unloadTextures(XYFontHandle font, GSGLOBAL* gs) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return;
    if (!s_font_slots[font].textures) return;

    for (int i = 0; i < s_font_slots[font].texture_count; i++) {
        s_font_slots[font].textures[i].unload(gs);
    }

    delete[] s_font_slots[font].textures;
    s_font_slots[font].textures = nullptr;
    s_font_slots[font].texture_count = 0;
}

GSTEXTURE* XYFont::getPageTexture(XYFontHandle font, int page_index) {
    if (font >= MAX_FONTS || s_font_slots[font].type == FONT_TYPE_NONE) return nullptr;
    if (!s_font_slots[font].textures || page_index < 0 || page_index >= s_font_slots[font].texture_count) return nullptr;
    return s_font_slots[font].textures[page_index].raw();
}

} // namespace xy
