#include "xy_game.hpp"

class InputStatusExample : public xy::XYGame {
public:
    void onRender() override {
        char buttons[96];

        graphics().drawText(28, 28, "JOYPAD STATUS", xy::Color(255, 255, 90), 3.0f);

        for (int pad = 0; pad < 2; ++pad) {
            const xy::XYPadState& state = input().pad(pad);
            int y = 92 + pad * 112;
            
            graphics().drawFormat(36, (float)y, xy::Color(230, 240, 255), 2.0f, nullptr, 
                                  "PAD %d: %s", pad + 1, state.connected ? "CONNECTED" : "DISCONNECTED");

            xy::appendPressedButtons(buttons, sizeof(buttons), state.buttons);
            graphics().drawFormat(36, (float)y + 28, xy::Color(160, 255, 190), 2.0f, nullptr, 
                                  "BUTTONS: %s", buttons);

            graphics().drawFormat(36, (float)y + 56, xy::Color(180, 210, 255), 2.0f, nullptr, 
                                  "LX:%03d LY:%03d RX:%03d RY:%03d",
                                  state.leftX, state.leftY, state.rightX, state.rightY);
        }
    }
};

int main() {
    InputStatusExample game;
    return game.run();
}
