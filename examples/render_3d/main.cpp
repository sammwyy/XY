// render_3d — XY Engine 3D demo
//
// Demonstrates:
//   - XYCamera (perspective, lookAt)
//   - XYLightSystem (directional + point light)
//   - XYMaterial (Phong shading)
//   - XYRenderer3D (CPU path)
//   - Primitives: cube, sphere, plane
//   - Input to orbit the camera

#include "xy_game.hpp"
#include "xy_input.hpp"
#include "xy_math.hpp"
#include "graphics/xy_graphics.hpp"
#include "graphics/xy_camera.hpp"
#include "graphics/xy_light.hpp"
#include "graphics/xy_material.hpp"
#include "graphics/xy_renderer3d.hpp"
#include "mesh/xy_mesh.hpp"
#include "mesh/xy_primitives.hpp"

#include <cstdio>

using namespace xy;

// ---------------------------------------------------------------------------
// Game subclass
// ---------------------------------------------------------------------------

class Game3D : public XYGame {
protected:
    bool onInit()       override;
    void onUpdate(float dt) override;
    void onRender()     override;
    void onShutdown()   override;

private:
    XYCamera      camera_;
    XYLightSystem lights_;
    XYRenderer3D* renderer_ = nullptr;

    XYMesh* cubeMesh_   = nullptr;
    XYMesh* sphereMesh_ = nullptr;
    XYMesh* planeMesh_  = nullptr;

    XYMaterial matCube_;
    XYMaterial matSphere_;
    XYMaterial matPlane_;

    float orbitAngle_  = 0.0f;
    float orbitHeight_ = 2.5f;
    float orbitRadius_ = 6.0f;
    float cubeRot_     = 0.0f;
    float sphereRot_   = 0.0f;
};

// ---------------------------------------------------------------------------
// onInit
// ---------------------------------------------------------------------------

bool Game3D::onInit() {
    // --- Meshes ---
    cubeMesh_   = primitives::createCube(0.75f);
    sphereMesh_ = primitives::createSphere(0.6f, 14, 20);
    planeMesh_  = primitives::createPlane(8.0f, 8.0f, 4, 4);

    cubeMesh_->calcFlatNormals();
    sphereMesh_->calcSmoothNormals();

    // --- Materials ---
    matCube_.diffuse   = {0.8f, 0.3f, 0.2f};
    matCube_.ambient   = {0.2f, 0.08f, 0.05f};
    matCube_.specular  = {0.9f, 0.9f, 0.9f};
    matCube_.shininess = 32.0f;

    matSphere_.diffuse   = {0.2f, 0.5f, 0.9f};
    matSphere_.ambient   = {0.05f, 0.12f, 0.22f};
    matSphere_.specular  = {1.0f, 1.0f, 1.0f};
    matSphere_.shininess = 64.0f;

    matPlane_ = XYMaterial::flat(0.3f, 0.6f, 0.3f);

    // --- Lights ---
    // Directional key light
    lights_.enableLight(0, LightType::Directional);
    lights_.light(0).direction = Vec3{-1.0f, -1.5f, -1.0f}.normalized();
    lights_.light(0).diffuse   = {1.0f, 0.95f, 0.9f};
    lights_.light(0).specular  = {1.0f, 1.0f,  1.0f};
    lights_.light(0).ambient   = {0.05f, 0.05f, 0.05f};

    // Point fill light
    lights_.enableLight(1, LightType::Point);
    lights_.light(1).position       = {3.0f, 2.0f, 2.0f};
    lights_.light(1).diffuse        = {0.6f, 0.5f, 0.3f};
    lights_.light(1).linearAtten    = 0.1f;
    lights_.light(1).quadraticAtten = 0.03f;

    lights_.setGlobalAmbient({0.08f, 0.08f, 0.12f});

    // --- Camera ---
    camera_.setAspect(graphics().width(), graphics().height());
    camera_.setFovDeg(60.0f);
    camera_.setNearFar(0.1f, 100.0f);

    // --- Renderer ---
    renderer_ = new XYRenderer3D(&graphics());

    return true;
}

// ---------------------------------------------------------------------------
// onUpdate
// ---------------------------------------------------------------------------

void Game3D::onUpdate(float dt) {
    XYInput& inp = input();

    if (inp.down(0, XY_BUTTON_LEFT))  orbitAngle_  -= 1.5f * dt;
    if (inp.down(0, XY_BUTTON_RIGHT)) orbitAngle_  += 1.5f * dt;
    if (inp.down(0, XY_BUTTON_UP))    orbitHeight_ += 2.0f * dt;
    if (inp.down(0, XY_BUTTON_DOWN))  orbitHeight_ -= 2.0f * dt;

    orbitHeight_ = math::clamp(orbitHeight_, 0.5f, 8.0f);

    if (inp.down(0, XY_BUTTON_L1)) orbitRadius_ -= 2.0f * dt;
    if (inp.down(0, XY_BUTTON_R1)) orbitRadius_ += 2.0f * dt;

    orbitRadius_ = math::clamp(orbitRadius_, 2.0f, 15.0f);

    Vec3 camPos = {
        cosf(orbitAngle_) * orbitRadius_,
        orbitHeight_,
        sinf(orbitAngle_) * orbitRadius_
    };
    camera_.lookAt(camPos, Vec3::zero());

    cubeRot_   += 0.8f * dt;
    sphereRot_ += 0.4f * dt;
}

// ---------------------------------------------------------------------------
// onRender
// ---------------------------------------------------------------------------

void Game3D::onRender() {
    renderer_->begin(camera_, lights_);

    // Ground plane
    renderer_->drawMesh(*planeMesh_,
        Mat4::translate({0.0f, -1.2f, 0.0f}),
        matPlane_);

    // Rotating cube
    Mat4 cubeModel = Mat4::translate({-1.5f, 0.0f, 0.0f})
                   * Mat4::rotateY(cubeRot_)
                   * Mat4::rotateX(cubeRot_ * 0.6f);
    renderer_->drawMesh(*cubeMesh_, cubeModel, matCube_);

    // Rotating sphere
    Mat4 sphereModel = Mat4::translate({1.5f, 0.0f, 0.0f})
                     * Mat4::rotateY(sphereRot_);
    renderer_->drawMesh(*sphereMesh_, sphereModel, matSphere_);

    renderer_->flush();

    // HUD
    XYGraphics& gfx = graphics();
    gfx.drawFormat(8, 8,  Color(255,255,255), 1.0f,
                   "3D  TRIS:%d", renderer_->lastFrameTriangles());
    gfx.drawFormat(8, 18, Color(200,200,200), 1.0f,
                   "ANG:%.1f H:%.1f R:%.1f",
                   math::toDeg(orbitAngle_), orbitHeight_, orbitRadius_);
    gfx.drawFormat(8, 28, Color(160,160,160), 1.0f,
                   "DPAD=ORBIT  L1/R1=ZOOM");
}

// ---------------------------------------------------------------------------
// onShutdown
// ---------------------------------------------------------------------------

void Game3D::onShutdown() {
    delete renderer_;
    delete cubeMesh_;
    delete sphereMesh_;
    delete planeMesh_;
    renderer_  = nullptr;
    cubeMesh_  = nullptr;
    sphereMesh_= nullptr;
    planeMesh_ = nullptr;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
    Game3D game;
    return game.run();
}
