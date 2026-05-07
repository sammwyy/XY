#include "xy_font_fnt.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <string.h>

namespace xy {

static void parse_params(const std::string& line, std::vector<std::pair<std::string, std::string>>& params) {
    std::stringstream ss(line);
    std::string tag;
    ss >> tag; // tag like 'char'

    std::string pair;
    while (ss >> pair) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = pair.substr(0, pos);
            std::string val = pair.substr(pos + 1);
            // Remove quotes from val
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            params.push_back({key, val});
        }
    }
}

static int get_int(const std::vector<std::pair<std::string, std::string>>& params, const char* key, int def = 0) {
    for (const auto& p : params) {
        if (p.first == key) return std::stoi(p.second);
    }
    return def;
}

static std::string get_str(const std::vector<std::pair<std::string, std::string>>& params, const char* key) {
    for (const auto& p : params) {
        if (p.first == key) return p.second;
    }
    return "";
}

bool xy_font_fnt_load(const char* path, XYFontDataFNT* out_data) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        
        std::vector<std::pair<std::string, std::string>> params;
        parse_params(line, params);

        if (line.compare(0, 4, "info") == 0) {
            // Unused for now
        } else if (line.compare(0, 6, "common") == 0) {
            out_data->metrics.line_height = get_int(params, "lineHeight");
            out_data->metrics.base = get_int(params, "base");
            out_data->scaleW = get_int(params, "scaleW", 1);
            out_data->scaleH = get_int(params, "scaleH", 1);
            // ascent/descent often not in FNT, use base
            out_data->metrics.ascent = out_data->metrics.base;
            out_data->metrics.descent = out_data->metrics.line_height - out_data->metrics.base;
        } else if (line.compare(0, 4, "page") == 0) {
            out_data->pages.push_back(get_str(params, "file"));
        } else if (line.compare(0, 4, "char") == 0) {
            XYFontGlyph g;
            g.codepoint = get_int(params, "id");
            g.x = get_int(params, "x");
            g.y = get_int(params, "y");
            g.width = get_int(params, "width");
            g.height = get_int(params, "height");
            g.x_offset = get_int(params, "xoffset");
            g.y_offset = get_int(params, "yoffset");
            g.x_advance = get_int(params, "xadvance");
            g.page = get_int(params, "page");
            
            g.u0 = (float)g.x / (float)out_data->scaleW;
            g.v0 = (float)g.y / (float)out_data->scaleH;
            g.u1 = (float)(g.x + g.width) / (float)out_data->scaleW;
            g.v1 = (float)(g.y + g.height) / (float)out_data->scaleH;

            out_data->glyphs.push_back(g);
        } else if (line.compare(0, 7, "kerning") == 0) {
            XYFontKerning k;
            k.first = get_int(params, "first");
            k.second = get_int(params, "second");
            k.amount = get_int(params, "amount");
            out_data->kernings.push_back(k);
        }
    }

    // Sort for binary search
    std::sort(out_data->glyphs.begin(), out_data->glyphs.end(), [](const XYFontGlyph& a, const XYFontGlyph& b) {
        return a.codepoint < b.codepoint;
    });

    std::sort(out_data->kernings.begin(), out_data->kernings.end(), [](const XYFontKerning& a, const XYFontKerning& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });

    return true;
}

void xy_font_fnt_unload(XYFontDataFNT* data) {
    data->glyphs.clear();
    data->kernings.clear();
    data->pages.clear();
}

const XYFontGlyph* xy_font_fnt_find_glyph(XYFontDataFNT* data, uint32_t codepoint) {
    auto it = std::lower_bound(data->glyphs.begin(), data->glyphs.end(), codepoint, [](const XYFontGlyph& g, uint32_t cp) {
        return g.codepoint < cp;
    });

    if (it != data->glyphs.end() && it->codepoint == codepoint) {
        return &(*it);
    }
    return nullptr;
}

int xy_font_fnt_get_kerning(XYFontDataFNT* data, uint32_t first, uint32_t second) {
    auto it = std::lower_bound(data->kernings.begin(), data->kernings.end(), std::make_pair(first, second), [](const XYFontKerning& k, const std::pair<uint32_t, uint32_t>& p) {
        if (k.first != p.first) return k.first < p.first;
        return k.second < p.second;
    });

    if (it != data->kernings.end() && it->first == first && it->second == second) {
        return it->amount;
    }
    return 0;
}

} // namespace xy
