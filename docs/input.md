# Input System

Xy's input module wraps PS2 pad polling and exposes stable per-frame button states for two controllers.

`XYGame` initializes input automatically and polls pads once per frame before `onUpdate(dt)`.

## Quick Usage

```cpp
void onUpdate(float) override {
    if (input().pressed(0, xy::XY_BUTTON_CROSS)) {
        jump();
    }

    if (input().down(0, xy::XY_BUTTON_LEFT)) {
        moveLeft();
    }

    if (input().released(0, xy::XY_BUTTON_START)) {
        openPauseMenu();
    }
}
```

Pad index `0` is controller 1. Pad index `1` is controller 2.

## Button States

| Method | Meaning |
|---|---|
| `down(pad, button)` | Button is currently held. |
| `pressed(pad, button)` | Button changed from up to down this frame. |
| `released(pad, button)` | Button changed from down to up this frame. |
| `pad(index)` | Full `XYPadState` snapshot. |

## Buttons

```cpp
XY_BUTTON_LEFT
XY_BUTTON_DOWN
XY_BUTTON_RIGHT
XY_BUTTON_UP
XY_BUTTON_START
XY_BUTTON_R3
XY_BUTTON_L3
XY_BUTTON_SELECT
XY_BUTTON_SQUARE
XY_BUTTON_CROSS
XY_BUTTON_CIRCLE
XY_BUTTON_TRIANGLE
XY_BUTTON_R1
XY_BUTTON_L1
XY_BUTTON_R2
XY_BUTTON_L2
```

Buttons are bit flags stored in `XYPadState::buttons`, `pressed`, and `released`.

## Pad State

```cpp
const xy::XYPadState& pad = input().pad(0);

if (pad.connected) {
    u16 buttons = pad.buttons;
    u8 lx = pad.leftX;
    u8 ly = pad.leftY;
    u8 rx = pad.rightX;
    u8 ry = pad.rightY;
}
```

Analog sticks use the raw DualShock range. The center is usually around `128`.

## Displaying Pressed Buttons

```cpp
char buttons[96];
const xy::XYPadState& pad = input().pad(0);
xy::appendPressedButtons(buttons, sizeof(buttons), pad.buttons);

graphics().drawFormat(36, 36, xy::Color(255, 255, 255), 2.0f,
                      "PAD1: %s", buttons);
```

`appendPressedButtons` writes `"NONE"` when no button is down.

## Initialization Details

`XYInput::init()` loads:

- `rom0:SIO2MAN`
- `rom0:PADMAN`

Then it opens pad ports `0` and `1`, locks DualShock mode, and stores the current state in `XYPadState`.

Games using `XYGame` do not need to call `input().init()` manually.

## Example

See `examples/input_status`.

The example renders:

- Connection status for both pads.
- Current button names.
- Raw analog stick values.

## Guidelines

| Use case | Recommendation |
|---|---|
| One-shot actions | `pressed` |
| Movement or charging | `down` |
| Menu release actions | `released` |
| Debug pad display | `pad()` + `appendPressedButtons` |

Read input in `onUpdate`, then render the result in `onRender`.
