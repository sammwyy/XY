#include "xy_game.hpp"

#include <cstdio>

class InputStatusExample : public xy::XYGame {
public:
    void onRender() override {
        char line[128];
        char buttons[96];

        debugText().drawText(28, 28, "JOYPAD STATUS", xy::Color(255, 255, 90), 3);

        for (int pad = 0; pad < 2; ++pad) {
            const xy::XYPadState& state = input().pad(pad);
            int y = 92 + pad * 112;
            std::snprintf(line, sizeof(line), "PAD %d: %s", pad + 1, state.connected ? "CONNECTED" : "DISCONNECTED");
            debugText().drawText(36, y, line, xy::Color(230, 240, 255), 2);

            xy::appendPressedButtons(buttons, sizeof(buttons), state.buttons);
            std::snprintf(line, sizeof(line), "BUTTONS: %s", buttons);
            debugText().drawText(36, y + 28, line, xy::Color(160, 255, 190), 2);

            std::snprintf(line, sizeof(line), "LX:%03d LY:%03d RX:%03d RY:%03d",
                          state.leftX, state.leftY, state.rightX, state.rightY);
            debugText().drawText(36, y + 56, line, xy::Color(180, 210, 255), 2);
        }
    }
};

int main() {
    InputStatusExample game;
    return game.run();
}

