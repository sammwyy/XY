#pragma once

#include "xy_math.hpp"
#include "xy_camera.hpp"
#include "xy_light.hpp"
#include "xy_material.hpp"
#include "../mesh/xy_mesh.hpp"

#include <gsKit.h>

namespace xy {

class XYGraphics;

// ---------------------------------------------------------------------------
// DrawCall3D — a single render submission
// Equivalent to what PS2GL accumulates before calling DrawLinearArrays()
// ---------------------------------------------------------------------------

struct DrawCall3D {
    const XYMesh*     mesh     = nullptr;
    const XYMaterial* material = nullptr;
    Mat4              model    = Mat4::identity();
};

// ---------------------------------------------------------------------------
// XYRenderer3D
//
// CPU-side 3D renderer that submits triangles to gsKit.
//
// Architecture (derived from PS2GL CLinearRenderer + CBaseRenderer):
//
//  begin(camera, lights)
//    ↓ caches VP matrix, pre-transforms light positions to "batch" space
//  submit(DrawCall3D) × N
//    ↓ queues draw calls (like PS2GL accumulates geometry in CGeometryBlock)
//  flush()
//    ↓ for each draw call:
//      1. Compute MVP = VP * model
//      2. Compute normalMat = inverse-transpose of model (PS2GL's objToWorldXfrmTrans)
//      3. For each triangle: transform verts → project → calc lighting → emit GS prim
//
// VU1 path (future): flush() detects if VU1 microcode is loaded and
// delegates to XYRenderer3D_VU1::flush() instead of CPU path.
// The same DrawCall3D queue is reused — only the backend changes.
// ---------------------------------------------------------------------------

class XYRenderer3D {
public:
    explicit XYRenderer3D(XYGraphics* gfx);
    ~XYRenderer3D() = default;

    // --- Frame lifecycle ---

    // Call after XYGraphics::beginFrame()
    // Sets up VP matrix and pre-transforms lights
    void begin(const XYCamera& camera, const XYLightSystem& lights);

    // Queue a draw call
    void submit(const DrawCall3D& call);

    // Shorthand for submit()
    void drawMesh(const XYMesh& mesh, const Mat4& model, const XYMaterial& material);

    // Process the queue and emit to GS. Call before XYGraphics::endFrame()
    void flush();

    // --- Stats (debug) ---
    int lastFrameTriangles() const { return lastTris_; }
    int lastFrameDrawCalls() const { return lastDC_;   }

private:
    // Transforms, lights, shading for one draw call (CPU path)
    void processDraw(const DrawCall3D& dc);

    // Project a world-space point to GS screen coordinates
    // Returns false if the point is behind the camera (clip)
    bool projectVertex(const Vec4& clipPos, float& sx, float& sy, float& sz) const;

    // Emit one triangle to the GS (flat-shaded or Gouraud)
    // All positions in GS screen space [0..width, 0..height]
    void emitTriangle(
        float x0, float y0, float z0, u64 col0,
        float x1, float y1, float z1, u64 col1,
        float x2, float y2, float z2, u64 col2);

    void emitTriangleTextured(
        float x0, float y0, float z0, float u0, float v0, u64 col0,
        float x1, float y1, float z1, float u1, float v1, u64 col1,
        float x2, float y2, float z2, float u2, float v2, u64 col2,
        GSTEXTURE* tex);

    XYGraphics*              gfx_;
    GSGLOBAL*                gs_     = nullptr;
    const XYCamera*          cam_    = nullptr;
    const XYLightSystem*     lights_ = nullptr;
    Mat4                     vp_;       // cached VP = proj * view
    float                    halfW_  = 320.0f;
    float                    halfH_  = 224.0f;

    // Draw call queue (cleared each flush)
    static const int MAX_DRAW_CALLS = 256;
    DrawCall3D drawQueue_[MAX_DRAW_CALLS];
    int        drawCount_ = 0;

    // Stats
    int lastTris_ = 0;
    int lastDC_   = 0;
};

} // namespace xy
