#include "xy_game.hpp"

#include <kernel.h>
#include <sifrpc.h>

namespace xy {

XYGame::XYGame()
    : running_(false), initialized_(false), graphics_(), input_(), audio_(), tasks_() {}

XYGame::~XYGame() {}

int XYGame::run() {
    SifInitRpc(0);

    if (!graphics_.init()) {
        return 1;
    }
    input_.init();
    audio_.init();

    initialized_ = onInit();
    if (!initialized_) {
        onShutdown();
        return 1;
    }

    running_ = true;
    while (running_) {
        input_.update();
        audio_.update();

        const float dt = 1.0f / 60.0f;
        onUpdate(dt);
        tasks_.update(dt);
        graphics_.beginFrame();
        onRender();
        graphics_.endFrame();
    }

    onShutdown();
    tasks_.shutdown();
    audio_.shutdown();
    graphics_.shutdown();
    return 0;
}

void XYGame::quit() {
    running_ = false;
}

XYGraphics& XYGame::graphics() {
    return graphics_;
}

XYInput& XYGame::input() {
    return input_;
}

XYAudio& XYGame::audio() {
    return audio_;
}

XYTasks& XYGame::tasks() {
    return tasks_;
}

bool XYGame::onInit() {
    return true;
}

void XYGame::onUpdate(float) {}

void XYGame::onRender() {}

void XYGame::onShutdown() {}

} // namespace xy
