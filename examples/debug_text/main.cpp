#include "xy_game.hpp"

class DebugTextExample : public xy::XYGame {
public:
    void onRender() override {
        char buttons[96];

        graphics().drawText(40, 36, "HELLO WORLD!", xy::Color(255, 240, 90), 4.0f);
        graphics().drawText(42, 100, "XY GRAPHICS TEXT OVERLAY", xy::Color(230, 240, 255), 2.0f);
        graphics().drawText(42, 130, "5X7 PIXELART GLYPHS", xy::Color(160, 255, 190), 2.0f);
        graphics().drawText(42, 160, "NUMBERS: 0123456789", xy::Color(180, 210, 255), 2.0f);

        const xy::XYPadState& pad = input().pad(0);
        xy::appendPressedButtons(buttons, sizeof(buttons), pad.buttons);
        graphics().drawFormat(42, 220, xy::Color(255, 255, 255), 2.0f, "PAD1: %s", buttons);

        graphics().drawText(42, 256, "USE THIS OVERLAY FROM ANY XYGAME", xy::Color(255, 170, 170), 2.0f);
    }
};

int main() {
    DebugTextExample game;
    return game.run();
}
