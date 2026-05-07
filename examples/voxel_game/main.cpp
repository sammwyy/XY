// voxel_game - simple first-person voxel floor

#include "xy_game.hpp"
#include "xy_input.hpp"
#include "xy_math.hpp"
#include "graphics/xy_camera.hpp"
#include "graphics/xy_light.hpp"
#include "graphics/xy_material.hpp"
#include "graphics/xy_renderer3d.hpp"
#include "mesh/xy_mesh.hpp"

#include <cmath>

using namespace xy;

namespace {

constexpr int WORLD_W = 16;
constexpr int WORLD_H = 1;
constexpr int WORLD_D = 16;
constexpr float BLOCK_SIZE = 1.0f;
constexpr float WORLD_MIN_X = -8.0f;
constexpr float WORLD_MAX_X = 8.0f;
constexpr float WORLD_MIN_Z = -8.0f;
constexpr float WORLD_MAX_Z = 8.0f;
constexpr float FLOOR_TOP_Y = 1.0f;

float stickAxis(u8 value, bool invert = false) {
    float axis = ((float)value - 128.0f) / 127.0f;
    if (invert) {
        axis = -axis;
    }
    return fabsf(axis) < 0.18f ? 0.0f : axis;
}

bool blockAt(int x, int y, int z) {
    return x >= 0 && x < WORLD_W &&
           y >= 0 && y < WORLD_H &&
           z >= 0 && z < WORLD_D;
}

void addFace(XYMesh& mesh,
             const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3,
             const Vec3& normal, const Color& color) {
    u32 base = (u32)mesh.vertexCount();
    mesh.addVertex(Vertex(p0, normal, {0.0f, 0.0f}, color));
    mesh.addVertex(Vertex(p1, normal, {1.0f, 0.0f}, color));
    mesh.addVertex(Vertex(p2, normal, {1.0f, 1.0f}, color));
    mesh.addVertex(Vertex(p3, normal, {0.0f, 1.0f}, color));
    mesh.addTriangle(base + 0, base + 1, base + 2);
    mesh.addTriangle(base + 0, base + 2, base + 3);
}

XYMesh* createVoxelFloorMesh() {
    XYMesh* mesh = new XYMesh();

    const Color topColor(78, 174, 84);
    const Color sideColor(116, 84, 52);
    const Color sideDarkColor(86, 62, 42);
    const Color bottomColor(48, 42, 38);

    for (int z = 0; z < WORLD_D; ++z) {
        for (int x = 0; x < WORLD_W; ++x) {
            const float x0 = WORLD_MIN_X + (float)x * BLOCK_SIZE;
            const float x1 = x0 + BLOCK_SIZE;
            const float y0 = 0.0f;
            const float y1 = FLOOR_TOP_Y;
            const float z0 = WORLD_MIN_Z + (float)z * BLOCK_SIZE;
            const float z1 = z0 + BLOCK_SIZE;

            addFace(*mesh, {x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0},
                    {0.0f, 1.0f, 0.0f}, topColor);
            addFace(*mesh, {x0, y0, z1}, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
                    {0.0f, -1.0f, 0.0f}, bottomColor);
            addFace(*mesh, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1},
                    {0.0f, 0.0f, 1.0f}, sideColor);
            addFace(*mesh, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
                    {0.0f, 0.0f, -1.0f}, sideDarkColor);
            addFace(*mesh, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1},
                    {1.0f, 0.0f, 0.0f}, sideDarkColor);
            addFace(*mesh, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0},
                    {-1.0f, 0.0f, 0.0f}, sideColor);
        }
    }

    mesh->calcBounds();
    return mesh;
}

class VoxelFacePredicate : public XYRenderPredicate {
public:
    bool shouldDrawFace(const RenderFaceContext& face) const override {
        Vec3 n = dominantAxis(face.localNormal);
        Vec3 ownerPoint = face.localCenter - n * 0.02f;
        Vec3 neighborPoint = face.localCenter + n * 0.02f;

        int ox = blockX(ownerPoint.x);
        int oy = blockY(ownerPoint.y);
        int oz = blockZ(ownerPoint.z);
        int nx = blockX(neighborPoint.x);
        int ny = blockY(neighborPoint.y);
        int nz = blockZ(neighborPoint.z);

        if (!blockAt(ox, oy, oz)) {
            return true;
        }
        return !blockAt(nx, ny, nz);
    }

private:
    static int blockX(float x) {
        return (int)floorf(x - WORLD_MIN_X);
    }

    static int blockY(float y) {
        return (int)floorf(y);
    }

    static int blockZ(float z) {
        return (int)floorf(z - WORLD_MIN_Z);
    }

    static Vec3 dominantAxis(const Vec3& n) {
        float ax = fabsf(n.x);
        float ay = fabsf(n.y);
        float az = fabsf(n.z);

        if (ax >= ay && ax >= az) {
            return {n.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
        }
        if (ay >= ax && ay >= az) {
            return {0.0f, n.y >= 0.0f ? 1.0f : -1.0f, 0.0f};
        }
        return {0.0f, 0.0f, n.z >= 0.0f ? 1.0f : -1.0f};
    }
};

Vec3 horizontalForward(float yaw) {
    return {sinf(yaw), 0.0f, -cosf(yaw)};
}

Vec3 horizontalRight(float yaw) {
    return {cosf(yaw), 0.0f, sinf(yaw)};
}

Vec3 cameraForward(float yaw, float pitch) {
    float cp = cosf(pitch);
    return {sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp};
}

} // namespace

class VoxelGame : public XYGame {
protected:
    bool onInit() override;
    void onUpdate(float dt) override;
    void onRender() override;
    void onShutdown() override;

private:
    XYCamera camera_;
    XYLightSystem lights_;
    XYRenderer3D* renderer_ = nullptr;
    XYMesh* worldMesh_ = nullptr;
    XYMaterial worldMaterial_;
    VoxelFacePredicate voxelPredicate_;

    Vec3 playerPos_ = {0.0f, FLOOR_TOP_Y, 4.0f}; // feet position
    float playerVy_ = 0.0f;
    float yaw_ = 0.0f;
    float pitch_ = -0.15f;
    bool onGround_ = true;

    void updateCamera();
    void movePlayer(float dt, const XYPadState& pad);
    void applyFloorCollision();
};

bool VoxelGame::onInit() {
    worldMesh_ = createVoxelFloorMesh();
    worldMaterial_ = XYMaterial::unlit();
    worldMaterial_.alpha = 1.0f;

    lights_.setEnabled(false);

    camera_.setAspect(graphics().width(), graphics().height());
    camera_.setFovDeg(65.0f);
    camera_.setNearFar(0.08f, 80.0f);
    updateCamera();

    renderer_ = new XYRenderer3D(&graphics());
    return true;
}

void VoxelGame::onUpdate(float dt) {
    XYInput& inp = input();
    const XYPadState& pad = inp.pad(0);

    yaw_ += stickAxis(pad.rightX) * 2.4f * dt;
    pitch_ += stickAxis(pad.rightY, true) * 1.8f * dt;

    if (inp.down(0, XY_BUTTON_LEFT))  yaw_ -= 2.0f * dt;
    if (inp.down(0, XY_BUTTON_RIGHT)) yaw_ += 2.0f * dt;
    if (inp.down(0, XY_BUTTON_UP))    pitch_ += 1.5f * dt;
    if (inp.down(0, XY_BUTTON_DOWN))  pitch_ -= 1.5f * dt;

    pitch_ = math::clamp(pitch_, math::toRad(-78.0f), math::toRad(78.0f));

    if (onGround_ && inp.pressed(0, XY_BUTTON_CROSS)) {
        playerVy_ = 5.2f;
        onGround_ = false;
    }

    movePlayer(dt, pad);

    playerVy_ -= 12.0f * dt;
    playerPos_.y += playerVy_ * dt;
    applyFloorCollision();

    if (inp.pressed(0, XY_BUTTON_START)) {
        quit();
    }

    updateCamera();
}

void VoxelGame::onRender() {
    renderer_->begin(camera_, lights_);
    renderer_->drawMesh(*worldMesh_, Mat4::identity(), worldMaterial_, &voxelPredicate_);
    renderer_->flush();

    const float cx = graphics().width() * 0.5f;
    const float cy = graphics().height() * 0.5f;
    graphics().drawRect(cx - 6.0f, cy, 12.0f, 1.0f, Color(255, 255, 255, 128));
    graphics().drawRect(cx, cy - 6.0f, 1.0f, 12.0f, Color(255, 255, 255, 128));
    graphics().drawFormat(16.0f, 16.0f, Color(255, 255, 255, 128), 1.0f,
                           "Voxel floor 16x1x16  L-stick move  R-stick look  X jump");
    graphics().drawFormat(16.0f, 32.0f, Color(180, 220, 255, 128), 1.0f,
                           "pos %.1f %.1f %.1f  tris %d culled %d",
                           playerPos_.x, playerPos_.y, playerPos_.z,
                           renderer_->lastFrameTriangles(),
                           renderer_->lastFrameCulledTriangles());
}

void VoxelGame::onShutdown() {
    delete renderer_;
    delete worldMesh_;
    renderer_ = nullptr;
    worldMesh_ = nullptr;
}

void VoxelGame::movePlayer(float dt, const XYPadState& pad) {
    Vec3 movement = Vec3::zero();
    movement += horizontalRight(yaw_) * stickAxis(pad.leftX);
    movement += horizontalForward(yaw_) * stickAxis(pad.leftY, true);

    if (movement.lengthSq() > 1.0f) {
        movement = movement.normalized();
    }

    const float speed = input().down(0, XY_BUTTON_R1) ? 5.2f : 3.0f;
    playerPos_ += movement * (speed * dt);

    const float radius = 0.28f;
    playerPos_.x = math::clamp(playerPos_.x, WORLD_MIN_X + radius, WORLD_MAX_X - radius);
    playerPos_.z = math::clamp(playerPos_.z, WORLD_MIN_Z + radius, WORLD_MAX_Z - radius);
}

void VoxelGame::applyFloorCollision() {
    if (playerPos_.y <= FLOOR_TOP_Y) {
        playerPos_.y = FLOOR_TOP_Y;
        playerVy_ = 0.0f;
        onGround_ = true;
    } else {
        onGround_ = false;
    }
}

void VoxelGame::updateCamera() {
    const Vec3 eye = playerPos_ + Vec3(0.0f, 1.55f, 0.0f);
    camera_.lookAt(eye, eye + cameraForward(yaw_, pitch_));
}

int main() {
    VoxelGame game;
    return game.run();
}
