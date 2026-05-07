#include "xy_game.hpp"

class AudioExample : public xy::XYGame {
public:
    bool onInit() override {
        bool ok = true;
        ok = audio().load("host:assets/bgm.snd", bgm_) && ok;
        ok = audio().load("host:assets/sfx_cross.snd", sfxCross_) && ok;
        ok = audio().load("host:assets/sfx_circle.snd", sfxCircle_) && ok;
        ok = audio().load("host:assets/sfx_square.snd", sfxSquare_) && ok;
        if (ok) {
            audio().playBgm(bgm_, true, 0.45f);
        }
        return ok;
    }

    void onUpdate(float) override {
        if (input().pressed(0, xy::XY_BUTTON_CROSS)) {
            audio().playSfx(sfxCross_, 1.0f);
        }
        if (input().pressed(0, xy::XY_BUTTON_CIRCLE)) {
            audio().playSfx(sfxCircle_, 1.0f);
        }
        if (input().pressed(0, xy::XY_BUTTON_SQUARE)) {
            audio().playSfx(sfxSquare_, 1.0f);
        }
    }

    void onRender() override {
        debugText().drawText(34, 44, "AUDIO EXAMPLE", xy::Color(255, 255, 80), 3);
        debugText().drawText(34, 90, "BGM LOOP: ASSETS/BGM.SND", xy::Color(230, 240, 255), 2);
        debugText().drawText(34, 125, "CROSS  CIRCLE  SQUARE", xy::Color(160, 255, 190), 2);
    }

private:
    xy::XYSound bgm_;
    xy::XYSound sfxCross_;
    xy::XYSound sfxCircle_;
    xy::XYSound sfxSquare_;
};

int main() {
    AudioExample game;
    return game.run();
}
