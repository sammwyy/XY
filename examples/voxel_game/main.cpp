// voxel_game - first-person voxel world with terrain, trees and rocks
// PS2-safe: world split into 4x4 chunks so only near chunks are drawn,
// keeping the per-frame GIF FIFO well within PS2 limits.

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

// ---------------------------------------------------------------------------
// World dimensions
// Keep small enough that total exposed faces stay under ~3000 triangles/frame.
// 16x16 columns x max height 5 = worst-case ~7680 exposed faces -> many
// will be culled by chunk distance.  With VIEW_RADIUS=2 chunks (8 blocks)
// the visible triangle count is typically well under 3000.
// ---------------------------------------------------------------------------

constexpr int   WORLD_W    = 16;
constexpr int   WORLD_H    = 6;    // max block height
constexpr int   WORLD_D    = 16;
constexpr float BLOCK_SIZE = 1.0f;
constexpr float WORLD_MIN_X = -(WORLD_W * 0.5f);  // -8
constexpr float WORLD_MIN_Z = -(WORLD_D * 0.5f);  // -8

// Chunk grid: each chunk covers CHUNK_SIZE x CHUNK_SIZE columns
constexpr int CHUNK_SIZE = 4;                          // blocks per chunk side
constexpr int CHUNKS_X   = WORLD_W / CHUNK_SIZE;      // 4
constexpr int CHUNKS_Z   = WORLD_D / CHUNK_SIZE;      // 4

// Maximum chunks drawn per frame (all 16 for a 4x4 grid - safe at this size)
constexpr int VIEW_RADIUS = 3;   // chunks (render up to VIEW_RADIUS away)

// ---------------------------------------------------------------------------
// Block types
// ---------------------------------------------------------------------------

enum BlockType : u8 {
    BLK_AIR    = 0,
    BLK_GRASS  = 1,   // pasto
    BLK_DIRT   = 2,   // tierra
    BLK_ROCK   = 3,   // roca
    BLK_TRUNK  = 4,   // tronco
    BLK_LEAVES = 5,   // hojas
};

static u8 world[WORLD_W][WORLD_H][WORLD_D];

// ---------------------------------------------------------------------------
// Block color palettes (top, bottom, N/S sides, E/W sides)
// ---------------------------------------------------------------------------

struct BlockColors { Color top, bottom, sideNS, sideEW; };

static const BlockColors PALETTE[] = {
    // AIR (unused)
    { {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0} },
    // GRASS
    { Color( 82,155, 48), Color( 60, 44,28), Color(100, 72,40), Color( 88, 62,34) },
    // DIRT
    { Color(110, 76, 44), Color( 70, 50,28), Color(104, 70,40), Color( 90, 60,34) },
    // ROCK
    { Color(148,148,148), Color( 90, 90,90), Color(125,125,125), Color(110,110,110) },
    // TRUNK
    { Color( 80, 52, 28), Color( 68, 44,24), Color(100, 64,32), Color( 88, 56,28) },
    // LEAVES
    { Color( 52,120, 34), Color( 40,100,28), Color( 48,110,30), Color( 44,105,28) },
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float stickAxis(u8 value, bool invert = false) {
    float axis = ((float)value - 128.0f) / 127.0f;
    if (invert) axis = -axis;
    return fabsf(axis) < 0.18f ? 0.0f : axis;
}

bool inBounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H && z >= 0 && z < WORLD_D;
}
bool isSolid(int x, int y, int z) {
    if (!inBounds(x, y, z)) return false;
    return world[x][y][z] != BLK_AIR;
}

// ---------------------------------------------------------------------------
// World generation
// ---------------------------------------------------------------------------

static int ihash(int x, int z, int seed = 0) {
    int n = x * 1619 + z * 31337 + seed * 6971;
    n = (n << 13) ^ n;
    return (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff;
}

static int terrainHeight(int x, int z) {
    int h = 1;
    h += ihash(x / 4, z / 4)     % 2;   // low-freq  (+0..1)
    h += ihash(x / 2, z / 2, 1)  % 2;   // mid-freq  (+0..1)
    if (h > 3) h = 3;
    return h;
}

static bool isRockPatch(int x, int z) {
    int rx = x % 8, rz = z % 8;
    return (rx >= 6 && rz >= 6);          // small corner clusters
}

static void placeTree(int tx, int tz) {
    if (tx < 1 || tx >= WORLD_W - 1 || tz < 1 || tz >= WORLD_D - 1) return;
    int baseY = terrainHeight(tx, tz);

    // Trunk: 2 blocks tall
    for (int dy = 0; dy < 2; ++dy) {
        int y = baseY + dy;
        if (y < WORLD_H) world[tx][y][tz] = BLK_TRUNK;
    }

    // Leaves: 3x3 at trunk top, then single tip
    int leafY = baseY + 1;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            int lx = tx + dx, lz = tz + dz, ly = leafY;
            if (inBounds(lx, ly, lz) && world[lx][ly][lz] == BLK_AIR)
                world[lx][ly][lz] = BLK_LEAVES;
        }
    }
    int tipY = baseY + 2;
    if (tipY < WORLD_H && world[tx][tipY][tz] == BLK_AIR)
        world[tx][tipY][tz] = BLK_LEAVES;
}

static void generateWorld() {
    for (int x = 0; x < WORLD_W; ++x)
        for (int y = 0; y < WORLD_H; ++y)
            for (int z = 0; z < WORLD_D; ++z)
                world[x][y][z] = BLK_AIR;

    for (int x = 0; x < WORLD_W; ++x) {
        for (int z = 0; z < WORLD_D; ++z) {
            int  h    = terrainHeight(x, z);
            bool rock = isRockPatch(x, z);
            for (int y = 0; y < h; ++y) {
                if (rock)          world[x][y][z] = BLK_ROCK;
                else if (y==h-1)   world[x][y][z] = BLK_GRASS;
                else               world[x][y][z] = BLK_DIRT;
            }
        }
    }

    // 4 trees in different quadrants
    placeTree(3,  3);
    placeTree(3,  12);
    placeTree(12, 3);
    placeTree(12, 12);
}

// ---------------------------------------------------------------------------
// Chunk meshes  (CHUNKS_X x CHUNKS_Z meshes, each covers CHUNK_SIZE columns)
// Building per-chunk lets us skip entire chunks that are too far away.
// ---------------------------------------------------------------------------

static void addFace(XYMesh& mesh,
                    const Vec3& p0, const Vec3& p1,
                    const Vec3& p2, const Vec3& p3,
                    const Vec3& normal, const Color& color) {
    u32 base = (u32)mesh.vertexCount();
    mesh.addVertex(Vertex(p0, normal, {0,0}, color));
    mesh.addVertex(Vertex(p1, normal, {1,0}, color));
    mesh.addVertex(Vertex(p2, normal, {1,1}, color));
    mesh.addVertex(Vertex(p3, normal, {0,1}, color));
    mesh.addTriangle(base, base+1, base+2);
    mesh.addTriangle(base, base+2, base+3);
}

// Build one chunk's mesh covering blocks [x0,x0+CHUNK_SIZE) x [z0,z0+CHUNK_SIZE)
static XYMesh* buildChunkMesh(int cx, int cz) {
    XYMesh* mesh = new XYMesh();
    int x0 = cx * CHUNK_SIZE;
    int z0 = cz * CHUNK_SIZE;

    for (int x = x0; x < x0 + CHUNK_SIZE; ++x) {
        for (int y = 0; y < WORLD_H; ++y) {
            for (int z = z0; z < z0 + CHUNK_SIZE; ++z) {
                u8 bt = world[x][y][z];
                if (bt == BLK_AIR) continue;

                const BlockColors& c = PALETTE[bt];
                float bx0 = WORLD_MIN_X + x * BLOCK_SIZE,  bx1 = bx0 + BLOCK_SIZE;
                float by0 = y * BLOCK_SIZE,                 by1 = by0 + BLOCK_SIZE;
                float bz0 = WORLD_MIN_Z + z * BLOCK_SIZE,  bz1 = bz0 + BLOCK_SIZE;

                if (!isSolid(x,y+1,z))
                    addFace(*mesh,{bx0,by1,bz0},{bx0,by1,bz1},{bx1,by1,bz1},{bx1,by1,bz0},{0,1,0},c.top);
                if (!isSolid(x,y-1,z))
                    addFace(*mesh,{bx0,by0,bz1},{bx0,by0,bz0},{bx1,by0,bz0},{bx1,by0,bz1},{0,-1,0},c.bottom);
                if (!isSolid(x,y,z+1))
                    addFace(*mesh,{bx0,by0,bz1},{bx1,by0,bz1},{bx1,by1,bz1},{bx0,by1,bz1},{0,0,1},c.sideNS);
                if (!isSolid(x,y,z-1))
                    addFace(*mesh,{bx1,by0,bz0},{bx0,by0,bz0},{bx0,by1,bz0},{bx1,by1,bz0},{0,0,-1},c.sideNS);
                if (!isSolid(x+1,y,z))
                    addFace(*mesh,{bx1,by0,bz1},{bx1,by0,bz0},{bx1,by1,bz0},{bx1,by1,bz1},{1,0,0},c.sideEW);
                if (!isSolid(x-1,y,z))
                    addFace(*mesh,{bx0,by0,bz0},{bx0,by0,bz1},{bx0,by1,bz1},{bx0,by1,bz0},{-1,0,0},c.sideEW);
            }
        }
    }

    mesh->calcBounds();
    return mesh;
}

// ---------------------------------------------------------------------------
// Camera/movement helpers
// ---------------------------------------------------------------------------

Vec3 horizontalForward(float yaw) { return {sinf(yaw), 0.0f, -cosf(yaw)}; }
Vec3 horizontalRight(float yaw)   { return {cosf(yaw), 0.0f,  sinf(yaw)}; }
Vec3 cameraForward(float yaw, float pitch) {
    float cp = cosf(pitch);
    return {sinf(yaw)*cp, sinf(pitch), -cosf(yaw)*cp};
}

static float groundAt(float wx, float wz) {
    int bx = (int)floorf((wx - WORLD_MIN_X) / BLOCK_SIZE);
    int bz = (int)floorf((wz - WORLD_MIN_Z) / BLOCK_SIZE);
    if (!inBounds(bx, 0, bz)) return 0.0f;
    for (int y = WORLD_H-1; y >= 0; --y)
        if (world[bx][y][bz] != BLK_AIR)
            return (float)(y+1) * BLOCK_SIZE;
    return 0.0f;
}

} // namespace

// ---------------------------------------------------------------------------
// Game class
// ---------------------------------------------------------------------------

class VoxelGame : public XYGame {
protected:
    bool onInit() override;
    void onUpdate(float dt) override;
    void onRender() override;
    void onShutdown() override;

private:
    XYCamera      camera_;
    XYLightSystem lights_;
    XYRenderer3D* renderer_  = nullptr;
    XYMaterial    worldMat_;

    // Chunk mesh grid
    XYMesh* chunks_[CHUNKS_X][CHUNKS_Z] = {};

    Vec3  playerPos_ = {0.0f, 3.0f, 2.0f};
    float playerVy_  = 0.0f;
    float yaw_       = 0.0f;
    float pitch_     = -0.15f;
    bool  onGround_  = false;

    void updateCamera();
    void movePlayer(float dt, const XYPadState& pad);
    void applyCollision();
};

bool VoxelGame::onInit() {
    generateWorld();

    for (int cx = 0; cx < CHUNKS_X; ++cx)
        for (int cz = 0; cz < CHUNKS_Z; ++cz)
            chunks_[cx][cz] = buildChunkMesh(cx, cz);

    worldMat_ = XYMaterial::unlit();
    worldMat_.alpha = 1.0f;
    lights_.setEnabled(false);

    camera_.setAspect(graphics().width(), graphics().height());
    camera_.setFovDeg(65.0f);
    camera_.setNearFar(0.1f, 60.0f);
    updateCamera();

    renderer_ = new XYRenderer3D(&graphics());
    return true;
}

void VoxelGame::onUpdate(float dt) {
    XYInput& inp = input();
    const XYPadState& pad = inp.pad(0);

    yaw_   += stickAxis(pad.rightX)       * 2.4f * dt;
    pitch_ += stickAxis(pad.rightY, true) * 1.8f * dt;

    if (inp.down(0, XY_BUTTON_LEFT))  yaw_   -= 2.0f * dt;
    if (inp.down(0, XY_BUTTON_RIGHT)) yaw_   += 2.0f * dt;
    if (inp.down(0, XY_BUTTON_UP))    pitch_ += 1.5f * dt;
    if (inp.down(0, XY_BUTTON_DOWN))  pitch_ -= 1.5f * dt;
    pitch_ = math::clamp(pitch_, math::toRad(-78.0f), math::toRad(78.0f));

    if (onGround_ && inp.pressed(0, XY_BUTTON_CROSS)) {
        playerVy_ = 6.0f;
        onGround_ = false;
    }

    movePlayer(dt, pad);
    playerVy_ -= 14.0f * dt;
    playerPos_.y += playerVy_ * dt;
    applyCollision();

    if (inp.pressed(0, XY_BUTTON_START)) quit();
    updateCamera();
}

void VoxelGame::onRender() {
    // Determine player's chunk coordinate
    float relX = playerPos_.x - WORLD_MIN_X;
    float relZ = playerPos_.z - WORLD_MIN_Z;
    int pcx = (int)(relX / (CHUNK_SIZE * BLOCK_SIZE));
    int pcz = (int)(relZ / (CHUNK_SIZE * BLOCK_SIZE));

    int drawnTris  = 0;
    int culledTris = 0;

    renderer_->begin(camera_, lights_);

    for (int cx = 0; cx < CHUNKS_X; ++cx) {
        for (int cz = 0; cz < CHUNKS_Z; ++cz) {
            // Skip chunks beyond VIEW_RADIUS
            int dx = cx - pcx, dz = cz - pcz;
            if (dx < 0) dx = -dx;
            if (dz < 0) dz = -dz;
            if (dx > VIEW_RADIUS || dz > VIEW_RADIUS) continue;

            XYMesh* m = chunks_[cx][cz];
            if (m && m->triangleCount() > 0) {
                renderer_->drawMesh(*m, Mat4::identity(), worldMat_);
            }
        }
    }

    renderer_->flush();
    drawnTris  = renderer_->lastFrameTriangles();
    culledTris = renderer_->lastFrameCulledTriangles();

    // Crosshair
    const float cx2 = graphics().width()  * 0.5f;
    const float cy2 = graphics().height() * 0.5f;
    graphics().drawRect(cx2-6, cy2,   12, 1, Color(255,255,255,180));
    graphics().drawRect(cx2,   cy2-6,  1,12, Color(255,255,255,180));

    // HUD
    graphics().drawFormat(8, 8, Color(255,255,255,180), 1.0f,
                          "Voxel  L-stick move  R-stick look  X jump  START quit");
    graphics().drawFormat(8, 22, Color(180,220,255,180), 1.0f,
                          "pos %.1f %.1f %.1f  tris %d culled %d",
                          playerPos_.x, playerPos_.y, playerPos_.z,
                          drawnTris, culledTris);
}

void VoxelGame::onShutdown() {
    delete renderer_;
    renderer_ = nullptr;
    for (int cx = 0; cx < CHUNKS_X; ++cx)
        for (int cz = 0; cz < CHUNKS_Z; ++cz) {
            delete chunks_[cx][cz];
            chunks_[cx][cz] = nullptr;
        }
}

void VoxelGame::movePlayer(float dt, const XYPadState& pad) {
    Vec3 mv = Vec3::zero();
    mv += horizontalRight(yaw_)   * stickAxis(pad.leftX);
    mv += horizontalForward(yaw_) * stickAxis(pad.leftY, true);
    if (mv.lengthSq() > 1.0f) mv = mv.normalized();

    const float speed = input().down(0, XY_BUTTON_R1) ? 6.0f : 3.5f;
    playerPos_ += mv * (speed * dt);

    playerPos_.x = math::clamp(playerPos_.x, WORLD_MIN_X + 0.3f, WORLD_MIN_X + WORLD_W - 0.3f);
    playerPos_.z = math::clamp(playerPos_.z, WORLD_MIN_Z + 0.3f, WORLD_MIN_Z + WORLD_D - 0.3f);
}

void VoxelGame::applyCollision() {
    float floor = groundAt(playerPos_.x, playerPos_.z);
    if (playerPos_.y <= floor) {
        playerPos_.y = floor;
        playerVy_    = 0.0f;
        onGround_    = true;
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
