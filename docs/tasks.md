# Task System

Xy's task system provides two execution modes:

1. **Main-thread timers**: `run`, `run_delay`, `run_repeat`.
2. **EE async threads**: `run_async`, `run_delay_async`, `run_repeat_async`.

Both modes return an `XYTaskId` and are controlled through the same `cancel`, `pause`, `resume`, and `list` API.

## Execution Model

| API | Thread | Best for |
|---|---|---|
| `run` | Main game thread | Run a callback on the next task update. |
| `run_delay` | Main game thread | UI/gameplay timers. |
| `run_repeat` | Main game thread | Counters, polling, periodic gameplay logic. |
| `run_async` | Separate EE thread | CPU work that should not block the frame. |
| `run_delay_async` | Separate EE thread | Delayed background work. |
| `run_repeat_async` | Separate EE thread | Background periodic jobs. |

Main-thread tasks are advanced by `XYGame` once per frame after `onUpdate(dt)` and before `onRender()`.

Async tasks use the EE kernel thread API (`CreateThread`, `StartThread`, `DelayThread`) and cooperate with the scheduler through short sleeps so `pause` and `cancel` can take effect.

## Basic Usage

```cpp
class MyGame : public xy::XYGame {
public:
    bool onInit() override {
        once_ = tasks().run(this, &MyGame::bootDone);
        delayed_ = tasks().run_delay(this, &MyGame::showMessage, 5000);
        repeat_ = tasks().run_repeat(this, &MyGame::tickSecond, 1000);
        return true;
    }

private:
    void bootDone() {}
    void showMessage() {}
    void tickSecond() {}

    xy::XYTaskId once_;
    xy::XYTaskId delayed_;
    xy::XYTaskId repeat_;
};
```

The callback can be a lambda, free function, or method pointer.

```cpp
tasks().run_delay([]() {
    // Runs on the main thread after 250 ms.
}, 250);
```

## Repeating Tasks

```cpp
// Repeat every 1000 ms forever.
xy::XYTaskId timer = tasks().run_repeat(this, &MyGame::tick, 1000);

// Start after 500 ms, then repeat every 1000 ms.
tasks().run_repeat(this, &MyGame::tick, 1000, 500);

// Repeat exactly 3 times.
tasks().run_repeat(this, &MyGame::tick, 1000, 0, 3);
```

`repeatCount == -1` means repeat forever.

## Async Tasks

Use `_async` when the callback should run outside the main game loop.

```cpp
tasks().run_async([]() {
    // Runs in a separate EE thread.
});

tasks().run_delay_async([]() {
    // Runs in a separate EE thread after 1000 ms.
}, 1000);

tasks().run_repeat_async([]() {
    // Runs periodically in a separate EE thread.
}, 250);
```

Async tasks are useful for CPU-side work. Do not call `graphics()`, gsKit rendering, input polling, or other main-loop-only systems from async callbacks unless that subsystem explicitly documents thread safety.

For game state shared with the render thread, keep the data small and simple, or add your own synchronization around shared state.

## Control API

```cpp
xy::XYTaskId id = tasks().run_repeat(this, &MyGame::tick, 1000);

tasks().pause(id);
tasks().resume(id);
tasks().cancel(id);

tasks().pause_all();
tasks().resume_all();
tasks().cancel_all();
```

The camelCase variants are also available:

```cpp
tasks().runDelay(callback, 1000);
tasks().runRepeat(callback, 1000);
tasks().cancelAll();
tasks().pauseAll();
tasks().resumeAll();
```

## Listing Tasks

```cpp
std::vector<xy::XYTaskInfo> pending = tasks().list();
for (const xy::XYTaskInfo& task : pending) {
    // task.id
    // task.type
    // task.remainingMs
    // task.runCount
    // task.paused
    // task.async
}
```

`tasks().count()` returns the number of active pending tasks.

## Example

See `examples/tasks`.

The example creates:

- A repeating main-thread task that increments a seconds counter.
- A delayed main-thread task that changes text after 5 seconds.
- CROSS toggles `pause` / `resume` for the counter task.

## Guidelines

| Use case | Recommendation |
|---|---|
| UI text changes | `run_delay` |
| Gameplay timers | `run_repeat` |
| Delayed state transitions | `run_delay` |
| Loading/preparing CPU data | `run_async` |
| Background polling | `run_repeat_async` |
| Rendering or GS calls | Main thread only |

Keep main-thread callbacks short. A long callback blocks the frame because it runs inside the game loop.
