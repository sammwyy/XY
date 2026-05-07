#include "xy_game.hpp"

#include <string>

class TasksExample : public xy::XYGame {
public:
    bool onInit() override {
        seconds_ = 0;
        counterPaused_ = false;
        message_ = "THIS TEXT WILL CHANGE IN 5 SECONDS";

        counterTask_ = tasks().run_repeat(this, &TasksExample::tickSecond, 1000);
        textTask_ = tasks().run_delay(this, &TasksExample::changeMessage, 5000);

        return counterTask_ != xy::XY_TASK_INVALID && textTask_ != xy::XY_TASK_INVALID;
    }

    void onUpdate(float) override {
        if (input().pressed(0, xy::XY_BUTTON_CROSS)) {
            counterPaused_ = !counterPaused_;
            if (counterPaused_) {
                tasks().pause(counterTask_);
            } else {
                tasks().resume(counterTask_);
            }
        }
    }

    void onRender() override {
        graphics().drawText(34, 36, "ASYNC TASKS", xy::Color(255, 255, 90), 3.0f);
        graphics().drawFormat(34, 92, xy::Color(230, 240, 255), 2.0f,
                              "SECONDS: %d", seconds_);
        graphics().drawText(34, 132, message_, xy::Color(160, 255, 190), 2.0f);
        graphics().drawFormat(34, 190, xy::Color(180, 210, 255), 2.0f,
                              "TASKS PENDING: %d", tasks().count());
        graphics().drawText(34, 232,
                            counterPaused_ ? "CROSS: RESUME COUNTER" : "CROSS: PAUSE COUNTER",
                            xy::Color(255, 180, 180), 2.0f);
    }

private:
    void tickSecond() {
        ++seconds_;
    }

    void changeMessage() {
        message_ = "THE TEXT CHANGED AFTER 5 SECONDS";
    }

    int seconds_;
    bool counterPaused_;
    std::string message_;
    xy::XYTaskId counterTask_;
    xy::XYTaskId textTask_;
};

int main() {
    TasksExample game;
    return game.run();
}
