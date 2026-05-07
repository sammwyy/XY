#pragma once

#include <tamtypes.h>

namespace xy {

enum XYButton : u16 {
    XY_BUTTON_LEFT = 1 << 0,
    XY_BUTTON_DOWN = 1 << 1,
    XY_BUTTON_RIGHT = 1 << 2,
    XY_BUTTON_UP = 1 << 3,
    XY_BUTTON_START = 1 << 4,
    XY_BUTTON_R3 = 1 << 5,
    XY_BUTTON_L3 = 1 << 6,
    XY_BUTTON_SELECT = 1 << 7,
    XY_BUTTON_SQUARE = 1 << 8,
    XY_BUTTON_CROSS = 1 << 9,
    XY_BUTTON_CIRCLE = 1 << 10,
    XY_BUTTON_TRIANGLE = 1 << 11,
    XY_BUTTON_R1 = 1 << 12,
    XY_BUTTON_L1 = 1 << 13,
    XY_BUTTON_R2 = 1 << 14,
    XY_BUTTON_L2 = 1 << 15
};

struct XYPadState {
    bool connected;
    u16 buttons;
    u16 pressed;
    u16 released;
    u8 leftX;
    u8 leftY;
    u8 rightX;
    u8 rightY;

    XYPadState();
};

class XYInput {
public:
    XYInput();

    bool init();
    void update();

    const XYPadState& pad(int index) const;
    bool down(int padIndex, XYButton button) const;
    bool pressed(int padIndex, XYButton button) const;
    bool released(int padIndex, XYButton button) const;

private:
    bool openPad(int port);
    void pollPad(int port);
    void waitReady(int port, int slot) const;
    u16 convertButtons(u16 ps2Buttons) const;

    bool initialized_;
    XYPadState pads_[2];
    u8 padBuffers_[2][256] __attribute__((aligned(64)));
};

const char* buttonName(XYButton button);
int appendPressedButtons(char* out, int outSize, u16 buttons);

} // namespace xy

