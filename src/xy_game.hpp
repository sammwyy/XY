#pragma once

#include "xy_audio.hpp"
#include "async/xy_async.hpp"
#include "graphics/xy_graphics.hpp"
#include "xy_input.hpp"

namespace xy {

class XYGame {
public:
    XYGame();
    virtual ~XYGame();

    int run();
    void quit();

    XYGraphics& graphics();
    XYInput& input();
    XYAudio& audio();
    XYTasks& tasks();

protected:
    virtual bool onInit();
    virtual void onUpdate(float dt);
    virtual void onRender();
    virtual void onShutdown();

private:
    bool running_;
    bool initialized_;
    XYGraphics graphics_;
    XYInput input_;
    XYAudio audio_;
    XYTasks tasks_;
};

} // namespace xy
