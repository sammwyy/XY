#pragma once

#include "image/xy_image.hpp"
#include "xy_math.hpp"
#include "font/xy_font.h"

#include <gsKit.h>
#include <string>

namespace xy {

class XYGraphics {
public:
    XYGraphics();
    ~XYGraphics();

    bool init(int width = 640, int height = 448);
    void shutdown();

    void beginFrame(const Color& clearColor = Color(0, 0, 0, 128));
    void endFrame();
    void resetVram();

    // Textures
    void drawTexture(XYTexture& texture, float x, float y);
    void drawTexture(XYTexture& texture, float x, float y, float rotationRad);
    void drawTexture(XYTexture& texture, float x, float y, float width, float height,
                     const Color& tint = Color());
    void drawTexture(XYTexture& texture, float x, float y, float width, float height,
                     float rotationRad, const Color& tint = Color());
    void drawTextureRegion(XYTexture& texture, float srcX, float srcY, float srcW, float srcH,
                           float dstX, float dstY, float dstW, float dstH,
                           const Color& tint = Color());
    
    // Primitives
    void drawRect(float x, float y, float width, float height, const Color& color);

    // Text
    void drawText(float x, float y, const std::string& text, const Color& color = Color(255, 255, 255, 128),
                  float scale = 1.0f, XYFontHandle font = XY_FONT_INVALID_HANDLE);
    void drawFormat(float x, float y, const Color& color, float scale, const char* format, ...);
    void drawFormat(float x, float y, const Color& color, float scale, XYFontHandle font, const char* format, ...);

    int width() const;
    int height() const;
    GSGLOBAL* gs();

private:
    void drawDebugText(float x, float y, const std::string& text, const Color& color, float scale);

    GSGLOBAL* gs_;
    int width_;
    int height_;
    uint32_t vramStart_;
};

u64 toGsColor(const Color& color);

} // namespace xy
