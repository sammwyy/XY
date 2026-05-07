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

using namespace xy;

// ---------------------------------------------------------------------------
// Scene state
// ---------------------------------------------------------------------------

static XYCamera     camera;
static XYLightSystem lights;
static XYRenderer3D* renderer = nullptr;

// Meshes (owned)
static XYMesh* cubeMesh   = nullptr;
static XYMesh* sphereMesh = nullptr;
static XYMesh* planeMesh  = nullptr;

// Materials
static XYMaterial matCube;
static XYMaterial matSphere;
static XYMaterial matPlane;

// Camera orbit state
static float orbitAngle  = 0.0f;
static float orbitHeight = 2.5f;
static float orbitRadius = 6.0f;

// Rotation angles for scene objects
static float cubeRot   = 0.0f;
static float sphereRot = 0.0f;

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void xy::XYGame::init() {
    // --- Meshes ---
    cubeMesh   = primitives::createCube(0.75f);
    sphereMesh = primitives::createSphere(0.6f, 14, 20);
    planeMesh  = primitives::createPlane(8.0f, 8.0f, 4, 4);

    // Smooth normals for sphere (matches PS2GL smooth shading mode)
    sphereMesh->calcSmoothNormals();
    cubeMesh->calcFlatNormals();

    // --- Materials ---
    matCube.diffuse   = {0.8f, 0.3f, 0.2f};   // red-ish
    matCube.ambient   = {0.2f, 0.08f, 0.05f};
    matCube.specular  = {0.9f, 0.9f, 0.9f};
    matCube.shininess = 32.0f;

    matSphere.diffuse   = {0.2f, 0.5f, 0.9f};  // blue
    matSphere.ambient   = {0.05f, 0.12f, 0.22f};
    matSphere.specular  = {1.0f, 1.0f, 1.0f};
    matSphere.shininess = 64.0f;

    matPlane = XYMaterial::flat(0.3f, 0.6f, 0.3f); // green ground

    // --- Lights ---
    // Directional key light (like PS2GL light 0 — white, full diffuse+specular)
    lights.enableLight(0, LightType::Directional);
    lights.light(0).direction = Vec3{-1.0f, -1.5f, -1.0f}.normalized();
    lights.light(0).diffuse   = {1.0f, 0.95f, 0.9f};
    lights.light(0).specular  = {1.0f, 1.0f,  1.0f};
    lights.light(0).ambient   = {0.05f, 0.05f, 0.05f};

    // Point fill light (warm, no specular)
    lights.enableLight(1, LightType::Point);
    lights.light(1).position       = {3.0f, 2.0f, 2.0f};
    lights.light(1).diffuse        = {0.6f, 0.5f, 0.3f};
    lights.light(1).specular       = {0.0f, 0.0f, 0.0f};
    lights.light(1).linearAtten    = 0.1f;
    lights.light(1).quadraticAtten = 0.03f;

    lights.setGlobalAmbient({0.08f, 0.08f, 0.12f});

    // --- Camera ---
    camera.setAspect(graphics.width(), graphics.height());
    camera.setFovDeg(60.0f);
    camera.setNearFar(0.1f, 100.0f);

    // --- Renderer ---
    renderer = new XYRenderer3D(&graphics);
}

// ---------------------------------------------------------------------------
// update
// ---------------------------------------------------------------------------

void xy::XYGame::update(float dt) {
    // Orbit camera with left stick / D-pad
    if (input.isHeld(XYButton::Left))  orbitAngle -= 1.5f * dt;
    if (input.isHeld(XYButton::Right)) orbitAngle += 1.5f * dt;
    if (input.isHeld(XYButton::Up))    orbitHeight += 2.0f * dt;
    if (input.isHeld(XYButton::Down))  orbitHeight -= 2.0f * dt;
    orbitHeight = math::clamp(orbitHeight, 0.5f, 8.0f);

    // Zoom with L1/R1
    if (input.isHeld(XYButton::L1)) orbitRadius -= 2.0f * dt;
    if (input.isHeld(XYButton::R1)) orbitRadius += 2.0f * dt;
    orbitRadius = math::clamp(orbitRadius, 2.0f, 15.0f);

    // Update camera position (orbit around origin)
    Vec3 camPos = {
        cosf(orbitAngle) * orbitRadius,
        orbitHeight,
        sinf(orbitAngle) * orbitRadius
    };
    camera.lookAt(camPos, Vec3::zero());

    // Rotate objects
    cubeRot   += 0.8f * dt;
    sphereRot += 0.4f * dt;
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------

void xy::XYGame::draw() {
    graphics.beginFrame(Color(15, 15, 25));  // dark blue-grey bg

    renderer->begin(camera, lights);

    // --- Ground plane ---
    renderer->drawMesh(*planeMesh, Mat4::translate({0.0f, -1.2f, 0.0f}), matPlane);

    // --- Rotating cube (left) ---
    Mat4 cubeModel = Mat4::translate({-1.5f, 0.0f, 0.0f})
                   * Mat4::rotateY(cubeRot)
                   * Mat4::rotateX(cubeRot * 0.6f);
    renderer->drawMesh(*cubeMesh, cubeModel, matCube);

    // --- Rotating sphere (right) ---
    Mat4 sphereModel = Mat4::translate({1.5f, 0.0f, 0.0f})
                     * Mat4::rotateY(sphereRot);
    renderer->drawMesh(*sphereMesh, sphereModel, matSphere);

    renderer->flush();

    // HUD
    graphics.drawFormat(8, 8, Color(255,255,255), 1.0f,
        "3D DEMO  TRIS:%d", renderer->lastFrameTriangles());
    graphics.drawFormat(8, 18, Color(200, 200, 200), 1.0f,
        "CAM ANG:%.1f  H:%.1f  R:%.1f",
        math::toDeg(orbitAngle), orbitHeight, orbitRadius);
    graphics.drawFormat(8, 28, Color(160, 160, 160), 1.0f,
        "DPAD=ORBIT  L1/R1=ZOOM");

    graphics.endFrame();
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void xy::XYGame::shutdown() {
    delete renderer;
    delete cubeMesh;
    delete sphereMesh;
    delete planeMesh;
}
