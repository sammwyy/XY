#include "xy_camera.hpp"

namespace xy {

XYCamera::XYCamera() {
    dirty_ = true;
}

void XYCamera::setPosition(const Vec3& pos) {
    position_ = pos;
    markDirty();
}

void XYCamera::setTarget(const Vec3& target) {
    target_ = target;
    markDirty();
}

void XYCamera::setUp(const Vec3& up) {
    up_ = up;
    markDirty();
}

void XYCamera::lookAt(const Vec3& pos, const Vec3& target, const Vec3& up) {
    position_ = pos;
    target_   = target;
    up_       = up;
    markDirty();
}

void XYCamera::setFovDeg(float fovDegrees) {
    fov_ = math::toRad(fovDegrees);
    markDirty();
}

void XYCamera::setFovRad(float fovRadians) {
    fov_ = fovRadians;
    markDirty();
}

void XYCamera::setAspect(float aspect) {
    aspect_ = aspect;
    markDirty();
}

void XYCamera::setAspect(int screenW, int screenH) {
    aspect_ = (float)screenW / (float)screenH;
    markDirty();
}

void XYCamera::setNearFar(float zNear, float zFar) {
    zNear_ = zNear;
    zFar_  = zFar;
    markDirty();
}

void XYCamera::markDirty() {
    dirty_ = true;
}

void XYCamera::recalculate() const {
    if (!dirty_) return;

    view_     = Mat4::lookAt(position_, target_, up_);
    proj_     = Mat4::perspective(fov_, aspect_, zNear_, zFar_);
    viewProj_ = proj_ * view_;

    // Extract frustum planes from VP matrix (Gribb-Hartmann method)
    // Each plane is stored as (nx, ny, nz, d) in Vec4
    const float* m = viewProj_.data();

    // Left:   col3 + col0
    frustumPlanes_[0] = { m[12]+m[0], m[13]+m[1], m[14]+m[2], m[15]+m[3] };
    // Right:  col3 - col0
    frustumPlanes_[1] = { m[12]-m[0], m[13]-m[1], m[14]-m[2], m[15]-m[3] };
    // Bottom: col3 + col1
    frustumPlanes_[2] = { m[12]+m[4], m[13]+m[5], m[14]+m[6], m[15]+m[7] };
    // Top:    col3 - col1
    frustumPlanes_[3] = { m[12]-m[4], m[13]-m[5], m[14]-m[6], m[15]-m[7] };
    // Near:   col3 + col2
    frustumPlanes_[4] = { m[12]+m[8], m[13]+m[9], m[14]+m[10], m[15]+m[11] };
    // Far:    col3 - col2
    frustumPlanes_[5] = { m[12]-m[8], m[13]-m[9], m[14]-m[10], m[15]-m[11] };

    dirty_ = false;
}

const Mat4& XYCamera::viewMatrix() const {
    recalculate();
    return view_;
}

const Mat4& XYCamera::projMatrix() const {
    recalculate();
    return proj_;
}

const Mat4& XYCamera::viewProjMatrix() const {
    recalculate();
    return viewProj_;
}

XYCamera::Ray XYCamera::screenToWorldRay(float ndcX, float ndcY) const {
    recalculate();

    // Unproject two points at near and far plane to get a world-space ray.
    // This is the inverse of what PS2GL's GetVertexXform() + GSScale does.
    // We use the inverse VP matrix to go from clip space back to world space.

    // Near point in clip space
    Vec4 nearClip = { ndcX, ndcY, -1.0f, 1.0f };
    // Far point
    Vec4 farClip  = { ndcX, ndcY,  1.0f, 1.0f };

    // Build inverse VP (affine approx — good enough for camera VP)
    // For a proper unproject, compute inv(VP) directly
    // Here we use the fact that inv(VP) = inv(V) * inv(P)
    // inv(P) for our perspective matrix can be computed analytically,
    // but for simplicity we use the affine inverse approach.

    // Simple approach: reconstruct from view and proj separately
    // inv(proj) * clip → view space
    const Mat4& p = proj_;
    // inv perspective: map (x,y,z,w) in clip to view space
    auto unprojectClip = [&](const Vec4& c) -> Vec3 {
        // x_view = x_clip * (1 / proj[0][0])
        // y_view = y_clip * (1 / proj[1][1])
        // z_view from z_clip and proj[2][2], proj[2][3]
        float xv = c.x / p.m[0][0];
        float yv = c.y / p.m[1][1];
        // z_ndc = c.z, w_clip = -z_view in RH perspective
        // z_view = proj[2][3] / (z_ndc - proj[2][2])
        float zv = p.m[2][3] / (c.z - p.m[2][2]);
        // Scale x,y by z
        xv *= -zv; yv *= -zv;
        return {xv, yv, zv};
    };

    Vec3 nearView = unprojectClip(nearClip);
    Vec3 farView  = unprojectClip(farClip);

    // Transform from view to world using inverse view = transposed rotation + negated trans
    Mat4 invView = view_.inversedAffine();

    Ray ray;
    ray.origin    = invView.transformPoint(nearView);
    Vec3 farWorld = invView.transformPoint(farView);
    ray.direction = (farWorld - ray.origin).normalized();
    return ray;
}

bool XYCamera::isPointVisible(const Vec3& p) const {
    recalculate();
    for (int i = 0; i < 6; i++) {
        const Vec4& plane = frustumPlanes_[i];
        if (plane.x*p.x + plane.y*p.y + plane.z*p.z + plane.w < 0.0f)
            return false;
    }
    return true;
}

bool XYCamera::isSphereVisible(const Vec3& center, float r) const {
    recalculate();
    for (int i = 0; i < 6; i++) {
        const Vec4& plane = frustumPlanes_[i];
        float dist = plane.x*center.x + plane.y*center.y + plane.z*center.z + plane.w;
        if (dist < -r) return false;
    }
    return true;
}

bool XYCamera::isBoxVisible(const Vec3& bMin, const Vec3& bMax) const {
    recalculate();
    for (int i = 0; i < 6; i++) {
        const Vec4& plane = frustumPlanes_[i];
        // Find the positive vertex (furthest in plane normal direction)
        Vec3 positive = {
            (plane.x >= 0) ? bMax.x : bMin.x,
            (plane.y >= 0) ? bMax.y : bMin.y,
            (plane.z >= 0) ? bMax.z : bMin.z
        };
        if (plane.x*positive.x + plane.y*positive.y + plane.z*positive.z + plane.w < 0.0f)
            return false;
    }
    return true;
}

} // namespace xy
