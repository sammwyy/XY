// render_3d - controllable 3D cube

#include "xy_game.hpp"
#include "xy_input.hpp"
#include "xy_math.hpp"
#include "graphics/xy_camera.hpp"
#include "graphics/xy_light.hpp"
#include "graphics/xy_material.hpp"
#include "graphics/xy_renderer3d.hpp"
#include "mesh/xy_mesh.hpp"
#include "mesh/xy_primitives.hpp"

#include <cmath>

using namespace xy;

namespace {

float stickAxis(u8 value, bool invert = false) {
    float axis = ((float)value - 128.0f) / 127.0f;
    if (invert) {
        axis = -axis;
    }
    return fabsf(axis) < 0.18f ? 0.0f : axis;
}

void paintCubeFaces(XYMesh& mesh) {
    const Color faceColors[6] = {
        Color(240, 70, 60),   // front
        Color(70, 130, 245),  // back
        Color(80, 220, 120),  // top
        Color(245, 220, 80),  // bottom
        Color(230, 90, 210),  // right
        Color(80, 230, 230),  // left
    };

    Vertex* vertices = mesh.vertices();
    for (int face = 0; face < 6; ++face) {
        for (int i = 0; i < 4; ++i) {
            vertices[face * 4 + i].color = faceColors[face];
        }
    }
}

} // namespace

class Game3D : public XYGame {
protected:
    bool onInit() override;
    void onUpdate(float dt) override;
    void onRender() override;
    void onShutdown() override;

private:
    XYCamera camera_;
    XYLightSystem lights_;
    XYRenderer3D* renderer_ = nullptr;

    XYMesh* cubeMesh_ = nullptr;
    XYMaterial cubeMaterial_;

    Vec3 cubePos_ = Vec3::zero();
    float cubeRotX_ = 0.35f;
    float cubeRotY_ = 0.55f;
    float zoom_ = 5.0f;

    void updateCamera();
};

bool Game3D::onInit() {
    cubeMesh_ = primitives::createCube(0.8f);
    paintCubeFaces(*cubeMesh_);

    cubeMaterial_ = XYMaterial::unlit();
    cubeMaterial_.alpha = 1.0f;
    lights_.setEnabled(false);

    camera_.setAspect(graphics().width(), graphics().height());
    camera_.setFovDeg(55.0f);
    camera_.setNearFar(0.1f, 100.0f);
    updateCamera();

    renderer_ = new XYRenderer3D(&graphics());
    return true;
}

void Game3D::onUpdate(float dt) {
    XYInput& inp = input();
    const XYPadState& pad = inp.pad(0);

    if (inp.down(0, XY_BUTTON_UP))    cubeRotX_ -= 2.2f * dt;
    if (inp.down(0, XY_BUTTON_DOWN))  cubeRotX_ += 2.2f * dt;
    if (inp.down(0, XY_BUTTON_LEFT))  cubeRotY_ -= 2.2f * dt;
    if (inp.down(0, XY_BUTTON_RIGHT)) cubeRotY_ += 2.2f * dt;

    if (inp.down(0, XY_BUTTON_L1)) zoom_ -= 3.0f * dt;
    if (inp.down(0, XY_BUTTON_R1)) zoom_ += 3.0f * dt;
    zoom_ = math::clamp(zoom_, 2.4f, 10.0f);

    cubePos_.x += stickAxis(pad.leftX) * 2.5f * dt;
    cubePos_.y += stickAxis(pad.leftY, true) * 2.5f * dt;
    cubePos_.x = math::clamp(cubePos_.x, -2.0f, 2.0f);
    cubePos_.y = math::clamp(cubePos_.y, -1.5f, 1.5f);

    updateCamera();
}

void Game3D::onRender() {
    renderer_->begin(camera_, lights_);

    Mat4 cubeModel = Mat4::translate(cubePos_)
                   * Mat4::rotateY(cubeRotY_)
                   * Mat4::rotateX(cubeRotX_);
    renderer_->drawMesh(*cubeMesh_, cubeModel, cubeMaterial_);

    renderer_->flush();
}

void Game3D::onShutdown() {
    delete renderer_;
    delete cubeMesh_;
    renderer_ = nullptr;
    cubeMesh_ = nullptr;
}

void Game3D::updateCamera() {
    camera_.lookAt({0.0f, 0.0f, zoom_}, Vec3::zero());
}

int main() {
    Game3D game;
    return game.run();
}
