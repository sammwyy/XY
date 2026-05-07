#pragma once

#include "image/xy_image.hpp"
#include "xy_math.hpp"

#include <gsKit.h>

namespace xy {

class XYGraphics {
public:
    XYGraphics();
    ~XYGraphics();

    bool init(int width = 640, int height = 448);
    void shutdown();

    void beginFrame(const Color& clearColor = Color(0, 0, 0, 128));
    void endFrame();

    void drawTexture(XYTexture& texture, float x, float y);
    void drawTexture(XYTexture& texture, float x, float y, float width, float height,
                     const Color& tint = Color());
    void drawRect(float x, float y, float width, float height, const Color& color);

    int width() const;
    int height() const;
    GSGLOBAL* gs();

private:
    GSGLOBAL* gs_;
    int width_;
    int height_;
};

u64 toGsColor(const Color& color);

} // namespace xy

