#include "xy_font_p2f.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace xy {

bool xy_font_p2f_load(const char* path, XYFontDataP2F* out_data) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < (long)sizeof(XYP2FHeader)) {
        fclose(f);
        return false;
    }

    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        fclose(f);
        return false;
    }

    fread(buffer, 1, size, f);
    fclose(f);

    XYP2FHeader* header = (XYP2FHeader*)buffer;

    // Validate magic
    if (memcmp(header->magic, "P2FN", 4) != 0) {
        free(buffer);
        return false;
    }

    if (header->version != 1) {
        free(buffer);
        return false;
    }

    out_data->buffer = buffer;
    out_data->header = *header;
    out_data->glyphs = (XYP2FGlyph*)(buffer + header->glyphs_offset);
    out_data->kernings = (XYP2FKerning*)(buffer + header->kernings_offset);
    out_data->pages = (XYP2FPage*)(buffer + header->pages_offset);
    out_data->string_table = (const char*)(buffer + header->strings_offset);

    return true;
}

void xy_font_p2f_unload(XYFontDataP2F* data) {
    if (data->buffer) {
        free(data->buffer);
        data->buffer = nullptr;
    }
}

const XYFontGlyph* xy_font_p2f_find_glyph(XYFontDataP2F* data, uint32_t codepoint) {
    int low = 0;
    int high = data->header.glyph_count - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (data->glyphs[mid].codepoint == codepoint) {
            return (const XYFontGlyph*)&data->glyphs[mid];
        }
        if (data->glyphs[mid].codepoint < codepoint) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return nullptr;
}

int xy_font_p2f_get_kerning(XYFontDataP2F* data, uint32_t first, uint32_t second) {
    if (data->header.kerning_count == 0) return 0;

    int low = 0;
    int high = data->header.kerning_count - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;

        // Using combined key for better binary search
        uint64_t key = ((uint64_t)first << 32) | second;
        uint64_t mid_key = ((uint64_t)data->kernings[mid].first << 32) | data->kernings[mid].second;

        if (mid_key == key) return data->kernings[mid].amount;
        if (mid_key < key) low = mid + 1;
        else high = mid - 1;
    }

    return 0;
}

} // namespace xy
