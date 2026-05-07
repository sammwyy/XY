#include "xy_game.hpp"

#include <cmath>

namespace {

float stickAxis(u8 value) {
    float axis = ((float)value - 128.0f) / 127.0f;
    return fabsf(axis) < 0.18f ? 0.0f : axis;
}

} // namespace

class RenderImagesExample : public xy::XYGame {
public:
    bool onInit() override {
        background_.load(graphics().gs(), "host:assets/background.ps2tex");
        sprite_.load(graphics().gs(), "host:assets/sprite.ps2tex");
        return background_.valid() && sprite_.valid();
    }

    void onUpdate(float dt) override {
        const xy::XYPadState& pad = input().pad(0);
        float turn = stickAxis(pad.leftX);

        if (input().down(0, xy::XY_BUTTON_LEFT)) {
            turn -= 1.0f;
        }
        if (input().down(0, xy::XY_BUTTON_RIGHT)) {
            turn += 1.0f;
        }

        imageRotation_ += turn * 2.8f * dt;
        if (imageRotation_ > xy::math::TWO_PI) {
            imageRotation_ -= xy::math::TWO_PI;
        } else if (imageRotation_ < -xy::math::TWO_PI) {
            imageRotation_ += xy::math::TWO_PI;
        }
    }

    void onRender() override {
        graphics().drawTexture(background_, 0, 0, graphics().width(), graphics().height());

        float x = (graphics().width() - sprite_.width()) * 0.5f;
        float y = (graphics().height() - sprite_.height()) * 0.5f;
        graphics().drawTexture(sprite_, x, y, imageRotation_);

        graphics().drawText(16, 16, "PNG SPRITE + JPG BACKGROUND", xy::Color(255, 255, 255), 2.0f);
        graphics().drawFormat(16, 40, xy::Color(255, 255, 120), 2.0f,
                              "ROTATION %.1f DEG", xy::math::toDeg(imageRotation_));
    }

private:
    xy::XYTexture background_;
    xy::XYTexture sprite_;
    float imageRotation_ = 0.0f;
};

int main() {
    RenderImagesExample game;
    return game.run();
}
