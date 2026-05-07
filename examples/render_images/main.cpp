#include "xy_game.hpp"

class RenderImagesExample : public xy::XYGame {
public:
    bool onInit() override {
        background_.load(graphics().gs(), "host:assets/background.ps2tex");
        sprite_.load(graphics().gs(), "host:assets/sprite.ps2tex");
        return background_.valid() && sprite_.valid();
    }

    void onRender() override {
        graphics().drawTexture(background_, 0, 0, graphics().width(), graphics().height());

        float x = (graphics().width() - sprite_.width()) * 0.5f;
        float y = (graphics().height() - sprite_.height()) * 0.5f;
        graphics().drawTexture(sprite_, x, y);

        graphics().drawText(16, 16, "PNG SPRITE + JPG BACKGROUND", xy::Color(255, 255, 255), 2.0f);
    }

private:
    xy::XYTexture background_;
    xy::XYTexture sprite_;
};

int main() {
    RenderImagesExample game;
    return game.run();
}
