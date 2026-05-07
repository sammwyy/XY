#include "xy_input.hpp"

#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <sifrpc.h>

#include <cstdio>
#include <cstring>

namespace xy {

XYPadState::XYPadState()
    : connected(false), buttons(0), pressed(0), released(0), leftX(128), leftY(128), rightX(128), rightY(128) {}

XYInput::XYInput() : initialized_(false), pads_(), padBuffers_() {
    std::memset(padBuffers_, 0, sizeof(padBuffers_));
}

bool XYInput::init() {
    if (initialized_) {
        return true;
    }

    SifLoadModule("rom0:SIO2MAN", 0, nullptr);
    SifLoadModule("rom0:PADMAN", 0, nullptr);
    padInit(0);

    openPad(0);
    openPad(1);
    initialized_ = true;
    return true;
}

void XYInput::update() {
    if (!initialized_) {
        return;
    }
    pollPad(0);
    pollPad(1);
}

const XYPadState& XYInput::pad(int index) const {
    return pads_[(index < 0 || index > 1) ? 0 : index];
}

bool XYInput::down(int padIndex, XYButton button) const {
    return (pad(padIndex).buttons & button) != 0;
}

bool XYInput::pressed(int padIndex, XYButton button) const {
    return (pad(padIndex).pressed & button) != 0;
}

bool XYInput::released(int padIndex, XYButton button) const {
    return (pad(padIndex).released & button) != 0;
}

bool XYInput::openPad(int port) {
    if (padPortOpen(port, 0, padBuffers_[port]) == 0) {
        pads_[port].connected = false;
        return false;
    }
    waitReady(port, 0);
    padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
    waitReady(port, 0);
    pads_[port].connected = true;
    return true;
}

void XYInput::pollPad(int port) {
    XYPadState& state = pads_[port];
    state.pressed = 0;
    state.released = 0;

    int padState = padGetState(port, 0);
    if (padState == PAD_STATE_DISCONN) {
        state.connected = false;
        state.buttons = 0;
        return;
    }

    if (padState != PAD_STATE_STABLE && padState != PAD_STATE_FINDCTP1) {
        return;
    }

    struct padButtonStatus status;
    std::memset(&status, 0, sizeof(status));
    if (padRead(port, 0, &status) == 0) {
        return;
    }

    u16 previous = state.buttons;
    state.buttons = convertButtons(status.btns);
    state.pressed = state.buttons & ~previous;
    state.released = previous & ~state.buttons;
    state.leftX = status.ljoy_h;
    state.leftY = status.ljoy_v;
    state.rightX = status.rjoy_h;
    state.rightY = status.rjoy_v;
    state.connected = true;
}

void XYInput::waitReady(int port, int slot) const {
    int state = 0;
    do {
        state = padGetState(port, slot);
    } while (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1);
}

u16 XYInput::convertButtons(u16 ps2Buttons) const {
    u16 raw = static_cast<u16>(~ps2Buttons);
    u16 out = 0;
    if (raw & PAD_LEFT) out |= XY_BUTTON_LEFT;
    if (raw & PAD_DOWN) out |= XY_BUTTON_DOWN;
    if (raw & PAD_RIGHT) out |= XY_BUTTON_RIGHT;
    if (raw & PAD_UP) out |= XY_BUTTON_UP;
    if (raw & PAD_START) out |= XY_BUTTON_START;
    if (raw & PAD_R3) out |= XY_BUTTON_R3;
    if (raw & PAD_L3) out |= XY_BUTTON_L3;
    if (raw & PAD_SELECT) out |= XY_BUTTON_SELECT;
    if (raw & PAD_SQUARE) out |= XY_BUTTON_SQUARE;
    if (raw & PAD_CROSS) out |= XY_BUTTON_CROSS;
    if (raw & PAD_CIRCLE) out |= XY_BUTTON_CIRCLE;
    if (raw & PAD_TRIANGLE) out |= XY_BUTTON_TRIANGLE;
    if (raw & PAD_R1) out |= XY_BUTTON_R1;
    if (raw & PAD_L1) out |= XY_BUTTON_L1;
    if (raw & PAD_R2) out |= XY_BUTTON_R2;
    if (raw & PAD_L2) out |= XY_BUTTON_L2;
    return out;
}

const char* buttonName(XYButton button) {
    switch (button) {
    case XY_BUTTON_LEFT: return "LEFT";
    case XY_BUTTON_DOWN: return "DOWN";
    case XY_BUTTON_RIGHT: return "RIGHT";
    case XY_BUTTON_UP: return "UP";
    case XY_BUTTON_START: return "START";
    case XY_BUTTON_R3: return "R3";
    case XY_BUTTON_L3: return "L3";
    case XY_BUTTON_SELECT: return "SELECT";
    case XY_BUTTON_SQUARE: return "SQUARE";
    case XY_BUTTON_CROSS: return "CROSS";
    case XY_BUTTON_CIRCLE: return "CIRCLE";
    case XY_BUTTON_TRIANGLE: return "TRIANGLE";
    case XY_BUTTON_R1: return "R1";
    case XY_BUTTON_L1: return "L1";
    case XY_BUTTON_R2: return "R2";
    case XY_BUTTON_L2: return "L2";
    }
    return "?";
}

int appendPressedButtons(char* out, int outSize, u16 buttons) {
    if (outSize <= 0) {
        return 0;
    }

    out[0] = '\0';
    int written = 0;
    const XYButton all[] = {
        XY_BUTTON_UP, XY_BUTTON_DOWN, XY_BUTTON_LEFT, XY_BUTTON_RIGHT, XY_BUTTON_CROSS, XY_BUTTON_CIRCLE,
        XY_BUTTON_SQUARE, XY_BUTTON_TRIANGLE, XY_BUTTON_L1, XY_BUTTON_R1, XY_BUTTON_L2, XY_BUTTON_R2,
        XY_BUTTON_L3, XY_BUTTON_R3, XY_BUTTON_SELECT, XY_BUTTON_START,
    };

    for (unsigned int i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        if ((buttons & all[i]) == 0) {
            continue;
        }
        const char* separator = written == 0 ? "" : " ";
        int count = std::snprintf(out + written, outSize - written, "%s%s", separator, buttonName(all[i]));
        if (count < 0) {
            break;
        }
        written += count;
        if (written >= outSize) {
            out[outSize - 1] = '\0';
            return outSize - 1;
        }
    }

    if (written == 0) {
        std::snprintf(out, outSize, "NONE");
    }
    return written;
}

} // namespace xy

