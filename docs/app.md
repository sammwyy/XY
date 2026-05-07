# App / Game Interface

`XYGame` is the main application interface. Games subclass it, override lifecycle methods, and call `run()` from `main()`.

## Minimal Game

```cpp
#include "xy_game.hpp"

class MyGame : public xy::XYGame {
public:
    bool onInit() override {
        return true;
    }

    void onUpdate(float dt) override {
        if (input().pressed(0, xy::XY_BUTTON_START)) {
            quit();
        }
    }

    void onRender() override {
        graphics().drawText(40, 40, "HELLO", xy::Color(255, 255, 255), 2.0f);
    }

    void onShutdown() override {}
};

int main() {
    MyGame game;
    return game.run();
}
```

## Lifecycle

`XYGame::run()` owns the engine lifecycle:

```text
SifInitRpc
graphics.init
input.init
audio.init
onInit

while running:
  input.update
  audio.update
  onUpdate(dt)
  tasks.update(dt)
  graphics.beginFrame
  onRender
  graphics.endFrame

onShutdown
tasks.shutdown
audio.shutdown
graphics.shutdown
```

`dt` is currently fixed at `1.0f / 60.0f`.

## Lifecycle Methods

| Method | Purpose |
|---|---|
| `onInit()` | Load resources and create initial state. Return `false` to abort. |
| `onUpdate(float dt)` | Per-frame gameplay logic. |
| `onRender()` | Draw the current frame. |
| `onShutdown()` | Release game-owned resources. |

## Engine Services

`XYGame` exposes core systems through references:

```cpp
graphics(); // XYGraphics
input();    // XYInput
audio();    // XYAudio
tasks();    // XYTasks
```

These are initialized before `onInit()` runs.

## Graphics

```cpp
graphics().drawRect(20, 20, 100, 40, xy::Color(255, 0, 0));
graphics().drawText(40, 80, "TEXT", xy::Color(255, 255, 255), 2.0f);
graphics().drawTexture(texture, x, y);
```

`beginFrame()` and `endFrame()` are called by `XYGame`; normal games should draw only inside `onRender()`.

## Input

```cpp
if (input().pressed(0, xy::XY_BUTTON_CROSS)) {
    // Button was pressed this frame.
}
```

Input is updated before `onUpdate()`, so state is fresh for each frame.

## Audio

```cpp
xy::XYSound sfx;
audio().load("host:assets/jump.snd", sfx);
audio().playSfx(sfx);
```

Audio is updated once per frame before `onUpdate()`.

## Tasks

```cpp
tasks().run_delay(this, &MyGame::showMessage, 1000);
tasks().run_repeat(this, &MyGame::tick, 1000);
tasks().run_async(this, &MyGame::backgroundWork);
```

Main-thread task timers are advanced after `onUpdate()` and before `onRender()`.

## Quitting

```cpp
void onUpdate(float) override {
    if (input().pressed(0, xy::XY_BUTTON_START)) {
        quit();
    }
}
```

`quit()` exits the main loop. Shutdown still runs normally.

## Resource Ownership

Load resources in `onInit()` and release them in `onShutdown()`.

```cpp
bool onInit() override {
    texture_.load(graphics().gs(), "host:assets/sprite.ps2tex");
    return true;
}

void onShutdown() override {
    texture_.unload(graphics().gs());
    texture_.free();
}
```

For font resources, unload font textures before graphics shuts down.

## Guidelines

| Work | Place |
|---|---|
| Resource loading | `onInit()` |
| Input and simulation | `onUpdate(dt)` |
| Drawing | `onRender()` |
| Cleanup | `onShutdown()` |
| Timed callbacks | `tasks()` |

Keep render code deterministic and avoid blocking calls inside `onRender()`.
