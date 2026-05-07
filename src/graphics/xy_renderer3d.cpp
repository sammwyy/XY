#include "xy_renderer3d.hpp"
#include "xy_graphics.hpp"
#include "../image/xy_image.hpp"

#include <gsKit.h>
#include <gsToolkit.h>

namespace xy {

bool XYRenderPredicate::shouldDrawMesh(const DrawCall3D&) const {
    return true;
}

bool XYRenderPredicate::shouldDrawFace(const RenderFaceContext&) const {
    return true;
}

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
    lastCulledTris_ = 0;

    if (gs_) {
        gsKit_set_test(gs_, GS_ZTEST_ON);
    }
}

void XYRenderer3D::submit(const DrawCall3D& call) {
    if (drawCount_ >= MAX_DRAW_CALLS) return;
    if (!call.mesh || call.mesh->vertexCount() == 0) return;
    drawQueue_[drawCount_++] = call;
}

void XYRenderer3D::drawMesh(const XYMesh& mesh, const Mat4& model, const XYMaterial& mat) {
    drawMesh(mesh, model, mat, nullptr);
}

void XYRenderer3D::drawMesh(const XYMesh& mesh, const Mat4& model, const XYMaterial& mat,
                            const XYRenderPredicate* predicate) {
    DrawCall3D dc;
    dc.mesh     = &mesh;
    dc.material = &mat;
    dc.model    = model;
    dc.predicate = predicate;
    submit(dc);
}

void XYRenderer3D::setRenderPredicate(const XYRenderPredicate* predicate) {
    predicate_ = predicate;
}

void XYRenderer3D::flush() {
    if (!gs_) return;

    lastDC_   = drawCount_;
    lastTris_ = 0;
    lastCulledTris_ = 0;

    for (int i = 0; i < drawCount_; i++) {
        processDraw(drawQueue_[i]);
    }
    drawCount_ = 0;

    gsKit_set_test(gs_, GS_ZTEST_OFF);
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

    if ((predicate_ && !predicate_->shouldDrawMesh(dc)) ||
        (dc.predicate && !dc.predicate->shouldDrawMesh(dc))) {
        lastCulledTris_ += mesh.triangleCount();
        return;
    }

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

        Vec3 localCenter = (va.position + vb.position + vc.position) / 3.0f;
        Vec3 localNormal = (va.normal + vb.normal + vc.normal).normalized();
        Vec3 worldCenter = dc.model.transformPoint(localCenter);
        Vec3 worldNormal = normalMat.transformDirection(localNormal).normalized();

        RenderFaceContext face;
        face.drawCall = &dc;
        face.triangleIndex = t;
        face.i0 = i0;
        face.i1 = i1;
        face.i2 = i2;
        face.v0 = &va;
        face.v1 = &vb;
        face.v2 = &vc;
        face.localCenter = localCenter;
        face.localNormal = localNormal;
        face.worldCenter = worldCenter;
        face.worldNormal = worldNormal;

        if ((predicate_ && !predicate_->shouldDrawFace(face)) ||
            (dc.predicate && !dc.predicate->shouldDrawFace(face))) {
            lastCulledTris_++;
            continue;
        }

        // --- Transform positions to clip space (MVP) ---
        Vec4 ca = mvp.transformVec4({va.position, 1.0f});
        Vec4 cb = mvp.transformVec4({vb.position, 1.0f});
        Vec4 cc = mvp.transformVec4({vc.position, 1.0f});

        // --- Conservative frustum cull ---
        // Reject only if ALL three vertices are behind the camera (w<=0).
        if (ca.w <= 0.0f && cb.w <= 0.0f && cc.w <= 0.0f) continue;
        // Reject if any vertex is behind the camera (cannot project safely).
        if (ca.w <= 0.0f || cb.w <= 0.0f || cc.w <= 0.0f) continue;

        // Compute NDC for conservative side/far plane test.
        float nx0 = ca.x / ca.w, nx1 = cb.x / cb.w, nx2 = cc.x / cc.w;
        float ny0 = ca.y / ca.w, ny1 = cb.y / cb.w, ny2 = cc.y / cc.w;
        float nz0 = ca.z / ca.w, nz1 = cb.z / cb.w, nz2 = cc.z / cc.w;

        // Skip only when ALL three verts are outside the same half-space.
        // This avoids popping when one vertex crosses the frustum edge.
        if (nx0 < -1.0f && nx1 < -1.0f && nx2 < -1.0f) continue; // all left
        if (nx0 >  1.0f && nx1 >  1.0f && nx2 >  1.0f) continue; // all right
        if (ny0 < -1.0f && ny1 < -1.0f && ny2 < -1.0f) continue; // all below
        if (ny0 >  1.0f && ny1 >  1.0f && ny2 >  1.0f) continue; // all above
        if (nz0 < -1.0f && nz1 < -1.0f && nz2 < -1.0f) continue; // all near
        if (nz0 >  1.0f && nz1 >  1.0f && nz2 >  1.0f) continue; // all far

        // --- Project to screen space ---
        float sx0, sy0, sz0, sx1, sy1, sz1, sx2, sy2, sz2;
        projectVertex(ca, sx0, sy0, sz0);
        projectVertex(cb, sx1, sy1, sz1);
        projectVertex(cc, sx2, sy2, sz2);

        // Clamp out-of-range projections to screen bounds so gsKit stays safe.
        float scrW = halfW_ * 2.0f - 1.0f;
        float scrH = halfH_ * 2.0f - 1.0f;
        sx0 = math::clamp(sx0, 0.0f, scrW); sy0 = math::clamp(sy0, 0.0f, scrH);
        sx1 = math::clamp(sx1, 0.0f, scrW); sy1 = math::clamp(sy1, 0.0f, scrH);
        sx2 = math::clamp(sx2, 0.0f, scrW); sy2 = math::clamp(sy2, 0.0f, scrH);

        // --- Lighting normals (CPU Phong) ---
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

    // Map NDC [-1,1] to screen space [0, width/height].
    // Y is flipped because GS Y axis goes down.
    sx = (ndcX + 1.0f) * halfW_;
    sy = (1.0f - ndcY) * halfH_;

    // GS_PSMZ_16S has 15 usable bits (max 0x7FFF).
    // GS clears depth to 0; ZTEST_ON passes when z_pixel >= z_buffer,
    // so near objects must have HIGHER z values than far objects.
    // ndcZ in [-1, 1]: near -> -1, far -> +1  (right-handed perspective)
    // We remap: near -> 0x7FFF, far -> 0, then store as integer.
    sz = (1.0f - ndcZ) * 0.5f; // [0, 1], near=1, far=0

    // Check if the projected pixel is within NDC range (visible).
    bool inRange = (ndcX >= -1.0f && ndcX <= 1.0f &&
                    ndcY >= -1.0f && ndcY <= 1.0f &&
                    ndcZ >= -1.0f && ndcZ <= 1.0f);
    return inRange;
}

void XYRenderer3D::emitTriangle(
    float x0, float y0, float z0, u64 col0,
    float x1, float y1, float z1, u64 col1,
    float x2, float y2, float z2, u64 col2)
{
    // GS_PSMZ_16S: 15 usable bits, max value 0x7FFF.
    // Multiply [0,1] depth by 0x7FFF so near (1.0) -> 0x7FFF,
    // far (0.0) -> 0. GS keeps the pixel that has the GREATER z.
    static const int GS_ZDEPTH_MAX = 0x7FFF;
    gsKit_prim_triangle_gouraud_3d(gs_,
        x0, y0, (int)(z0 * GS_ZDEPTH_MAX),
        x1, y1, (int)(z1 * GS_ZDEPTH_MAX),
        x2, y2, (int)(z2 * GS_ZDEPTH_MAX),
        col0, col1, col2);
}

void XYRenderer3D::emitTriangleTextured(
    float x0, float y0, float z0, float u0, float v0, u64 col0,
    float x1, float y1, float z1, float u1, float v1, u64 col1,
    float x2, float y2, float z2, float u2, float v2, u64 col2,
    GSTEXTURE* tex)
{
    static const int GS_ZDEPTH_MAX = 0x7FFF;
    gsKit_prim_triangle_goraud_texture_3d(gs_, tex,
        x0, y0, (int)(z0 * GS_ZDEPTH_MAX), u0 * tex->Width, v0 * tex->Height,
        x1, y1, (int)(z1 * GS_ZDEPTH_MAX), u1 * tex->Width, v1 * tex->Height,
        x2, y2, (int)(z2 * GS_ZDEPTH_MAX), u2 * tex->Width, v2 * tex->Height,
        col0, col1, col2);
}

} // namespace xy
