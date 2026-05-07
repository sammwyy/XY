#include "xy_renderer3d.hpp"
#include "xy_graphics.hpp"
#include "../image/xy_image.hpp"

#include <gsKit.h>
#include <gsToolkit.h>

namespace xy {

XYRenderer3D::XYRenderer3D(XYGraphics* gfx)
    : gfx_(gfx)
    , gs_(gfx ? gfx->gs() : nullptr)
{
}

void XYRenderer3D::begin(const XYCamera& camera, const XYLightSystem& lights) {
    cam_    = &camera;
    lights_ = &lights;
    vp_     = camera.viewProjMatrix();

    halfW_ = gfx_ ? (float)gfx_->width()  * 0.5f : 320.0f;
    halfH_ = gfx_ ? (float)gfx_->height() * 0.5f : 224.0f;

    drawCount_ = 0;
    lastTris_  = 0;
    lastDC_    = 0;
}

void XYRenderer3D::submit(const DrawCall3D& call) {
    if (drawCount_ >= MAX_DRAW_CALLS) return;
    if (!call.mesh || call.mesh->vertexCount() == 0) return;
    drawQueue_[drawCount_++] = call;
}

void XYRenderer3D::drawMesh(const XYMesh& mesh, const Mat4& model, const XYMaterial& mat) {
    DrawCall3D dc;
    dc.mesh     = &mesh;
    dc.material = &mat;
    dc.model    = model;
    submit(dc);
}

void XYRenderer3D::flush() {
    if (!gs_) return;

    lastDC_   = drawCount_;
    lastTris_ = 0;

    for (int i = 0; i < drawCount_; i++) {
        processDraw(drawQueue_[i]);
    }
    drawCount_ = 0;
}

// ---------------------------------------------------------------------------
// processDraw — the core render loop
//
// Mirrors PS2GL's pipeline:
//   CBaseRenderer::AddVu1RendererContext() — setup transforms + lights
//   CLinearRenderer::DrawBlock()           — per-vertex processing loop
//
// In the VU1 path (future), this function will:
//   1. Pack model matrix + lights into VU1 memory layout (kVertexXfrm etc.)
//   2. Upload mesh vertices via DMA to VU1 input buffer
//   3. Kick VU1 program (the equivalent of packet.Mscnt())
//   4. VU1 outputs transformed vertices directly to GS FIFO
//
// For now: full CPU transform + lighting + gsKit submission.
// ---------------------------------------------------------------------------

void XYRenderer3D::processDraw(const DrawCall3D& dc) {
    const XYMesh&     mesh = *dc.mesh;
    const XYMaterial& mat  = dc.material ? *dc.material : XYMaterial{};

    // Compute MVP = VP * Model (cached VP from begin())
    Mat4 mvp = vp_ * dc.model;

    // Normal matrix = inverse-transpose of Model upper-left 3x3
    // This is exactly what PS2GL's objToWorldXfrmTrans does in
    // base_renderer.cpp AddVu1RendererContext() before uploading to VU1
    Mat4 normalMat = dc.model.normalMatrix();

    // Camera position in world space (for specular eyeDir)
    const Vec3& camPos = cam_->position();

    int triCount = mesh.triangleCount();
    bool indexed  = mesh.isIndexed();
    const Vertex* verts   = mesh.vertices();
    const u32*    indices = mesh.indices();

    // Determine if we need to bind a texture
    GSTEXTURE* tex = (mat.texture) ? mat.texture->raw() : nullptr;
    if (tex) {
        gsKit_TexManager_bind(gs_, tex);
    }

    for (int t = 0; t < triCount; t++) {
        u32 i0, i1, i2;
        if (indexed) {
            i0 = indices[t*3+0];
            i1 = indices[t*3+1];
            i2 = indices[t*3+2];
        } else {
            i0 = t*3+0;
            i1 = t*3+1;
            i2 = t*3+2;
        }

        const Vertex& va = verts[i0];
        const Vertex& vb = verts[i1];
        const Vertex& vc = verts[i2];

        // --- Transform positions to clip space (MVP) ---
        Vec4 ca = mvp.transformVec4({va.position, 1.0f});
        Vec4 cb = mvp.transformVec4({vb.position, 1.0f});
        Vec4 cc = mvp.transformVec4({vc.position, 1.0f});

        // --- Project to screen space ---
        float sx0 = 0.0f, sy0 = 0.0f, sz0 = 0.0f;
        float sx1 = 0.0f, sy1 = 0.0f, sz1 = 0.0f;
        float sx2 = 0.0f, sy2 = 0.0f, sz2 = 0.0f;

        bool vis0 = projectVertex(ca, sx0, sy0, sz0);
        bool vis1 = projectVertex(cb, sx1, sy1, sz1);
        bool vis2 = projectVertex(cc, sx2, sy2, sz2);

        // Skip if all vertices are behind camera
        if (!vis0 && !vis1 && !vis2) continue;

        // --- Lighting (CPU Phong) ---
        // Transform normals using the normal matrix (inverse-transpose)
        Vec3 wn0 = normalMat.transformDirection(va.normal).normalized();
        Vec3 wn1 = normalMat.transformDirection(vb.normal).normalized();
        Vec3 wn2 = normalMat.transformDirection(vc.normal).normalized();

        // World-space positions for point/spot light attenuation
        Vec3 wp0 = dc.model.transformPoint(va.position);
        Vec3 wp1 = dc.model.transformPoint(vb.position);
        Vec3 wp2 = dc.model.transformPoint(vc.position);

        // Eye direction for specular
        Vec3 eye0 = (camPos - wp0).normalized();
        Vec3 eye1 = (camPos - wp1).normalized();
        Vec3 eye2 = (camPos - wp2).normalized();

        // Choose diffuse source (material vs per-vertex color)
        Vec3 diff0 = mat.useVertexColor
            ? Vec3{va.color.r/255.0f, va.color.g/255.0f, va.color.b/255.0f}
            : mat.diffuse;
        Vec3 diff1 = mat.useVertexColor
            ? Vec3{vb.color.r/255.0f, vb.color.g/255.0f, vb.color.b/255.0f}
            : mat.diffuse;
        Vec3 diff2 = mat.useVertexColor
            ? Vec3{vc.color.r/255.0f, vc.color.g/255.0f, vc.color.b/255.0f}
            : mat.diffuse;

        Color col0 = lights_->lightingEnabled()
            ? lights_->calcPhong(wp0, wn0, eye0, mat.ambient, diff0, mat.specular, mat.shininess)
            : Color::fromFloat(diff0.x, diff0.y, diff0.z);

        Color col1 = lights_->lightingEnabled()
            ? lights_->calcPhong(wp1, wn1, eye1, mat.ambient, diff1, mat.specular, mat.shininess)
            : Color::fromFloat(diff1.x, diff1.y, diff1.z);

        Color col2 = lights_->lightingEnabled()
            ? lights_->calcPhong(wp2, wn2, eye2, mat.ambient, diff2, mat.specular, mat.shininess)
            : Color::fromFloat(diff2.x, diff2.y, diff2.z);

        // Apply overall alpha from material
        u8 a = (u8)(mat.alpha * 255.0f);
        col0.a = a; col1.a = a; col2.a = a;

        // --- Emit to GS ---
        lastTris_++;
        if (tex) {
            emitTriangleTextured(
                sx0, sy0, sz0, va.uv.x, va.uv.y, GS_SETREG_RGBAQ(col0.r, col0.g, col0.b, col0.a>>1, 0),
                sx1, sy1, sz1, vb.uv.x, vb.uv.y, GS_SETREG_RGBAQ(col1.r, col1.g, col1.b, col1.a>>1, 0),
                sx2, sy2, sz2, vc.uv.x, vc.uv.y, GS_SETREG_RGBAQ(col2.r, col2.g, col2.b, col2.a>>1, 0),
                tex);
        } else {
            emitTriangle(
                sx0, sy0, sz0, GS_SETREG_RGBAQ(col0.r, col0.g, col0.b, col0.a>>1, 0),
                sx1, sy1, sz1, GS_SETREG_RGBAQ(col1.r, col1.g, col1.b, col1.a>>1, 0),
                sx2, sy2, sz2, GS_SETREG_RGBAQ(col2.r, col2.g, col2.b, col2.a>>1, 0));
        }
    }
}

bool XYRenderer3D::projectVertex(const Vec4& clip, float& sx, float& sy, float& sz) const {
    if (clip.w <= 0.0f) return false;

    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;
    float ndcZ = clip.z * invW;

    // Map NDC [-1,1] to screen space [0, width/height]
    // Y is flipped because GS Y axis goes down
    sx = (ndcX + 1.0f) * halfW_;
    sy = (1.0f - ndcY) * halfH_;
    sz = (ndcZ + 1.0f) * 0.5f;   // depth to [0,1]

    return (ndcZ >= -1.0f && ndcZ <= 1.0f);
}

void XYRenderer3D::emitTriangle(
    float x0, float y0, float z0, u64 col0,
    float x1, float y1, float z1, u64 col1,
    float x2, float y2, float z2, u64 col2)
{
    // Gouraud-shaded triangle via gsKit
    // gsKit_prim_triangle_gouraud_3d takes (x,y,z) in screen space
    gsKit_prim_triangle_gouraud_3d(gs_,
        x0, y0, (int)(z0 * 0xFFFFFF), col0,
        x1, y1, (int)(z1 * 0xFFFFFF), col1,
        x2, y2, (int)(z2 * 0xFFFFFF), col2);
}

void XYRenderer3D::emitTriangleTextured(
    float x0, float y0, float z0, float u0, float v0, u64 col0,
    float x1, float y1, float z1, float u1, float v1, u64 col1,
    float x2, float y2, float z2, float u2, float v2, u64 col2,
    GSTEXTURE* tex)
{
    gsKit_prim_triangle_goraud_texture_3d(gs_, tex,
        x0, y0, (int)(z0 * 0xFFFFFF), u0 * tex->Width, v0 * tex->Height, col0,
        x1, y1, (int)(z1 * 0xFFFFFF), u1 * tex->Width, v1 * tex->Height, col1,
        x2, y2, (int)(z2 * 0xFFFFFF), u2 * tex->Width, v2 * tex->Height, col2);
}

} // namespace xy
