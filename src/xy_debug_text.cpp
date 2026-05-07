#include "xy_debug_text.hpp"

#include <cstdarg>
#include <cstdio>

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

} // namespace

XYDebugText::XYDebugText(XYGraphics& graphics) : graphics_(graphics) {}

void XYDebugText::drawText(int x, int y, const char* text, const Color& color, int scale) {
    if (text == nullptr) {
        return;
    }

    int cursorX = x;
    int cursorY = y;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            cursorX = x;
            cursorY += 8 * scale;
            continue;
        }

        const u8* glyph = glyphFor(*p);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((glyph[row] & (1 << (4 - col))) != 0) {
                    graphics_.drawRect(cursorX + col * scale, cursorY + row * scale, scale, scale, color);
                }
            }
        }
        cursorX += 6 * scale;
    }
}

void XYDebugText::drawFormat(int x, int y, const Color& color, int scale, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    drawText(x, y, buffer, color, scale);
}

} // namespace xy

