#pragma once

#include <tamtypes.h>

namespace xy {

struct Vec2 {
    float x;
    float y;

    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}
};

struct Color {
    u8 r;
    u8 g;
    u8 b;
    u8 a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(u8 red, u8 green, u8 blue, u8 alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
};

} // namespace xy
