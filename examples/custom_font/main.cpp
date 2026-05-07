#include "xy_game.hpp"
#include "font/xy_font.h"

class CustomFontExample : public xy::XYGame {
public:
    bool onInit() override {
        // Initialize font system
        xy::XYFont::init();

        // Load the custom font (P2F or FNT)
        if (!xy::XYFont::load("host:assets/opensans.ps2fnt", &font_)) {
            return false;
        }

        // Load textures for the font
        xy::XYFont::loadTextures(font_, graphics().gs());

        return true;
    }

    void onShutdown() override {
        // Cleanup font
        xy::XYFont::unloadTextures(font_, graphics().gs());
        xy::XYFont::unload(font_);
        xy::XYFont::shutdown();
    }

    void onRender() override {
        // Render using the custom font
        graphics().drawText(50, 50, "Custom Font Rendering", xy::Color(255, 255, 255), 1.0f, font_);
        
        xy::XYFontMetrics metrics = xy::XYFont::getMetrics(font_);
        graphics().drawFormat(50, 100, xy::Color(255, 255, 100), 1.0f, font_, 
                              "LINE HEIGHT: %d\nBASE: %d", metrics.line_height, metrics.base);

        graphics().drawText(50, 200, "ABCDEFGHIJKLMN\nOPQRSTUVWXYZ\n0123456789", 
                            xy::Color(150, 255, 150), 0.8f, font_);

        // Comparison with fallback (debug) font
        graphics().drawText(50, 350, "FALLBACK FONT (DEBUG GLYPHS)", xy::Color(200, 200, 200), 2.0f);
    }

private:
    xy::XYFontHandle font_;
};

int main() {
    CustomFontExample game;
    return game.run();
}
