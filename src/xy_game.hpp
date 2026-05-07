#pragma once

#include "xy_audio.hpp"
#include "xy_debug_text.hpp"
#include "xy_graphics.hpp"
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
    XYDebugText& debugText();

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
    XYDebugText debugText_;
};

} // namespace xy

