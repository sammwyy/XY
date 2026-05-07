#pragma once

#include "xy_graphics.hpp"

namespace xy {

class XYDebugText {
public:
    explicit XYDebugText(XYGraphics& graphics);

    void drawText(int x, int y, const char* text, const Color& color = Color(235, 245, 255, 128),
                  int scale = 2);
    void drawFormat(int x, int y, const Color& color, int scale, const char* format, ...);

private:
    XYGraphics& graphics_;
};

} // namespace xy

