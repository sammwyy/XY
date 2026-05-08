#include "xy_graphics.hpp"
#include "../xy_alloc.hpp"
#include <dmaKit.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <kernel.h>
#include <vector>

namespace xy {

namespace {

const u8* glyphFor(char c) {
    static const u8 space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const u8 unknown[7] = {14, 17, 1, 2, 4, 0, 4};
    static const u8 glyphs[][7] = {
        {14, 17, 19, 21, 25, 17, 14}, // 0
        {4, 12, 4, 4, 4, 4, 14},      // 1
        {14, 17, 1, 2, 4, 8, 31},     // 2
        {30, 1, 1, 14, 1, 1, 30},     // 3
        {2, 6, 10, 18, 31, 2, 2},     // 4
        {31, 16, 16, 30, 1, 1, 30},   // 5
        {14, 16, 16, 30, 17, 17, 14}, // 6
        {31, 1, 2, 4, 8, 8, 8},       // 7
        {14, 17, 17, 14, 17, 17, 14}, // 8
        {14, 17, 17, 15, 1, 1, 14},   // 9
        {14, 17, 17, 31, 17, 17, 17}, // A
        {30, 17, 17, 30, 17, 17, 30}, // B
        {14, 17, 16, 16, 16, 17, 14}, // C
        {30, 17, 17, 17, 17, 17, 30}, // D
        {31, 16, 16, 30, 16, 16, 31}, // E
        {31, 16, 16, 30, 16, 16, 16}, // F
        {14, 17, 16, 23, 17, 17, 15}, // G
        {17, 17, 17, 31, 17, 17, 17}, // H
        {14, 4, 4, 4, 4, 4, 14},      // I
        {7, 2, 2, 2, 18, 18, 12},     // J
        {17, 18, 20, 24, 20, 18, 17}, // K
        {16, 16, 16, 16, 16, 16, 31}, // L
        {17, 27, 21, 21, 17, 17, 17}, // M
        {17, 25, 21, 19, 17, 17, 17}, // N
        {14, 17, 17, 17, 17, 17, 14}, // O
        {30, 17, 17, 30, 16, 16, 16}, // P
        {14, 17, 17, 17, 21, 18, 13}, // Q
        {30, 17, 17, 30, 20, 18, 17}, // R
        {15, 16, 16, 14, 1, 1, 30},   // S
        {31, 4, 4, 4, 4, 4, 4},       // T
        {17, 17, 17, 17, 17, 17, 14}, // U
        {17, 17, 17, 17, 17, 10, 4},  // V
        {17, 17, 17, 21, 21, 21, 10}, // W
        {17, 17, 10, 4, 10, 17, 17},  // X
        {17, 17, 10, 4, 4, 4, 4},     // Y
        {31, 1, 2, 4, 8, 16, 31},     // Z
    };

    if (c == ' ') return space;
    if (c >= '0' && c <= '9') return glyphs[c - '0'];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return glyphs[10 + c - 'A'];
    switch (c) {
    case ':': {
        static const u8 g[7] = {0, 4, 4, 0, 4, 4, 0};
        return g;
    }
    case '.': {
        static const u8 g[7] = {0, 0, 0, 0, 0, 12, 12};
        return g;
    }
    case '-': {
        static const u8 g[7] = {0, 0, 0, 31, 0, 0, 0};
        return g;
    }
    case '/': {
        static const u8 g[7] = {1, 1, 2, 4, 8, 16, 16};
        return g;
    }
    case '+': {
        static const u8 g[7] = {0, 4, 4, 31, 4, 4, 0};
        return g;
    }
    case '!': {
        static const u8 g[7] = {4, 4, 4, 4, 4, 0, 4};
        return g;
    }
    }
    return unknown;
}

int gsCoord(float value, int offset) {
    int coord = static_cast<int>(value * 16.0f) + offset;
    if (coord < 0) return 0;
    if (coord >= 4096 * 16) return 4096 * 16 - 1;
    return coord;
}

int gsUv(float value, int max) {
    int coord = static_cast<int>(value * 16.0f);
    if (coord < 0) return 0;
    if (coord > max) coord = max;
    if (coord >= 1024 * 16) return 1024 * 16 - 1;
    return coord;
}

gs_rgbaq makeRgbaq(u64 color) {
    gs_rgbaq out;
    out.color.rgbaq = color;
    out.tag = GS_RGBAQ;
    return out;
}

gs_uv makeUv(const GSTEXTURE* texture, float u, float v) {
    gs_uv out;
    out.coord.uv = GS_SETREG_UV(gsUv(u, texture->Width * 16), gsUv(v, texture->Height * 16));
    out.tag = GS_UV;
    return out;
}

gs_xyz2 makeXyz2(const GSGLOBAL* gs, float x, float y, int z) {
    gs_xyz2 out;
    out.xyz.xyz = GS_SETREG_XYZ2(gsCoord(x, gs->OffsetX), gsCoord(y, gs->OffsetY), z);
    out.tag = GS_XYZ2;
    return out;
}

} // namespace

u64 toGsColor(const Color &color) {
    return GS_SETREG_RGBAQ((color.r + 1) >> 1, (color.g + 1) >> 1, (color.b + 1) >> 1, (color.a + 1) >> 1, 0x00);
}

XYGraphics::XYGraphics() : gs_(nullptr), width_(640), height_(448) {}

XYGraphics::~XYGraphics() {
    shutdown();
}

bool XYGraphics::init(int width, int height) {
    width_ = width;
    height_ = height;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gs_ = gsKit_init_global();
    if (gs_ == nullptr) {
        return false;
    }

    gs_->Mode = GS_MODE_NTSC;
    gs_->Interlace = GS_INTERLACED;
    gs_->Field = GS_FIELD;
    gs_->Width = width_;
    gs_->Height = height_;
    gs_->PSM = GS_PSM_CT32;
    gs_->PSMZ = GS_PSMZ_16S;
    gs_->DoubleBuffering = GS_SETTING_ON;
    gs_->ZBuffering = GS_SETTING_ON;

    gsKit_init_screen(gs_);
    gsKit_mode_switch(gs_, GS_ONESHOT);
    
    vramStart_ = gs_->CurrentPointer;

    gs_->PrimAlphaEnable = GS_SETTING_ON;
    gs_->PrimAAEnable = GS_SETTING_ON;
    
    gsKit_set_test(gs_, GS_ATEST_ON);
    gsKit_set_primalpha(gs_, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gsKit_set_test(gs_, GS_ZTEST_OFF);

    return true;
}

void XYGraphics::shutdown() {
    gs_ = nullptr;
}

void XYGraphics::resetVram() {
    if (!gs_) return;
    std::printf("[VRAM] Resetting to 0x%08lx (was 0x%08lx)\n", (unsigned long)vramStart_, (unsigned long)gs_->CurrentPointer);
    gs_->CurrentPointer = vramStart_;
    XYVramAllocator::clear(gs_);
}

void XYGraphics::beginFrame(const Color &clearColor) {
    if (!gs_) return;
    gsKit_clear(gs_, toGsColor(clearColor));
}

void XYGraphics::endFrame() {
    if (!gs_) return;
    gsKit_queue_exec(gs_);
    gsKit_sync_flip(gs_);
}

void XYGraphics::drawTexture(XYTexture &texture, float x, float y) {
    drawTexture(texture, x, y, static_cast<float>(texture.width()), static_cast<float>(texture.height()),
                Color());
}

void XYGraphics::drawTexture(XYTexture &texture, float x, float y, float width, float height,
                             const Color &tint) {
    drawTexture(texture, x, y, width, height, 0.0f, tint);
}

void XYGraphics::drawTexture(XYTexture &texture, float x, float y, float rotationRad) {
    drawTexture(texture, x, y, static_cast<float>(texture.width()), static_cast<float>(texture.height()),
                rotationRad);
}

void XYGraphics::drawTexture(XYTexture &texture, float x, float y, float width, float height,
                             float rotationRad, const Color &tint) {
    if (!gs_ || !texture.valid()) return;

    GSTEXTURE *raw = texture.raw();
    if (!raw) return;

    if (fabsf(rotationRad) < 0.0001f) {
        gsKit_prim_sprite_texture(gs_, raw, x, y, 0.0f, 0.0f, x + width, y + height,
                                  static_cast<float>(texture.width()), static_cast<float>(texture.height()), 1,
                                  toGsColor(tint));
        return;
    }

    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float hw = width * 0.5f;
    const float hh = height * 0.5f;
    const float c = cosf(rotationRad);
    const float s = sinf(rotationRad);

    float px[4];
    float py[4];
    const float localX[4] = {-hw, hw, hw, -hw};
    const float localY[4] = {-hh, -hh, hh, hh};

    for (int i = 0; i < 4; ++i) {
        px[i] = cx + localX[i] * c - localY[i] * s;
        py[i] = cy + localX[i] * s + localY[i] * c;
    }

    const float u0 = 0.0f;
    const float v0 = 0.0f;
    const float u1 = static_cast<float>(texture.width());
    const float v1 = static_cast<float>(texture.height());
    const u64 color = toGsColor(tint);

    GSPRIMUVPOINT vertices[6];
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    const float u[4] = {u0, u1, u1, u0};
    const float v[4] = {v0, v0, v1, v1};

    for (int i = 0; i < 6; ++i) {
        const int index = indices[i];
        vertices[i].rgbaq = makeRgbaq(color);
        vertices[i].uv = makeUv(raw, u[index], v[index]);
        vertices[i].xyz2 = makeXyz2(gs_, px[index], py[index], 1);
    }

    gsKit_prim_list_triangle_goraud_texture_uv_3d(gs_, raw, 6, vertices);
}

void XYGraphics::drawTextureRegion(XYTexture& texture, float srcX, float srcY, float srcW, float srcH,
                                   float dstX, float dstY, float dstW, float dstH,
                                   const Color& tint) {
    if (!gs_ || !texture.valid()) return;

    GSTEXTURE* raw = texture.raw();
    if (!raw) return;

    gsKit_prim_sprite_texture(gs_, raw,
                               dstX, dstY,
                               srcX, srcY,
                               dstX + dstW, dstY + dstH,
                               srcX + srcW, srcY + srcH,
                               1, toGsColor(tint));
}

void XYGraphics::drawRect(float x, float y, float width, float height, const Color &color) {
    if (!gs_) return;
    gsKit_prim_sprite(gs_, x, y, x + width, y + height, 1, toGsColor(color));
}

void XYGraphics::drawText(float x, float y, const std::string& text, const Color& color, float scale, XYFontHandle font) {
    if (font == XY_FONT_INVALID_HANDLE) {
        drawDebugText(x, y, text, color, scale);
        return;
    }

    if (!gs_) return;

    XYTextLayoutParams params;
    params.x = x;
    params.y = y;
    params.scale = scale;
    params.color = toGsColor(color);
    params.spacing = 0;
    params.line_spacing = 0;

    static XYTextVertex vertices[1024];
    int count = XYFont::layoutText(font, text.c_str(), params, vertices, 1024);

    if (count == 0) return;

    for (int i = 0; i < count; i += 6) {
        GSTEXTURE* tex = XYFont::getPageTexture(font, vertices[i].page);
        if (!tex) continue;
        
        float x0 = vertices[i].x;
        float y0 = vertices[i].y;
        float x1 = vertices[i+4].x;
        float y1 = vertices[i+4].y;
        float u0 = vertices[i].u;
        float v0 = vertices[i].v;
        float u1 = vertices[i+4].u;
        float v1 = vertices[i+4].v;

        gsKit_prim_sprite_texture(gs_, tex, x0, y0, u0 * tex->Width, v0 * tex->Height, 
                                  x1, y1, u1 * tex->Width, v1 * tex->Height, 1, vertices[i].color);
    }
}

void XYGraphics::drawFormat(float x, float y, const Color& color, float scale, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    drawText(x, y, buffer, color, scale, XY_FONT_INVALID_HANDLE);
}

void XYGraphics::drawFormat(float x, float y, const Color& color, float scale, XYFontHandle font, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    drawText(x, y, buffer, color, scale, font);
}

void XYGraphics::drawDebugText(float x, float y, const std::string& text, const Color& color, float scale) {
    float cursorX = x;
    float cursorY = y;
    for (char c : text) {
        if (c == '\n') {
            cursorX = x;
            cursorY += 8 * scale;
            continue;
        }

        const u8* glyph = glyphFor(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((glyph[row] & (1 << (4 - col))) != 0) {
                    drawRect(cursorX + col * scale, cursorY + row * scale, scale, scale, color);
                }
            }
        }
        cursorX += 6 * scale;
    }
}

int XYGraphics::width() const { return width_; }
int XYGraphics::height() const { return height_; }
GSGLOBAL* XYGraphics::gs() { return gs_; }

} // namespace xy
