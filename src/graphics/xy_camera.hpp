#pragma once

#include "xy_math.hpp"

namespace xy {

// ---------------------------------------------------------------------------
// XYCamera
//
// Inspired by PS2GL's split ModelView/Projection matrix stacks and
// CImmDrawContext::GetVertexXform() which caches the combined MVP.
//
// Key design from PS2GL:
//  - viewProjMatrix() is cached and only recomputed when dirty (IsVertexXformValid)
//  - normalMatrix() is the inverse-transpose for correct normal transforms
//  - screenToWorldRay() replicates the unproject using the inverse VP
// ---------------------------------------------------------------------------

class XYCamera {
public:
    XYCamera();

    // --- Setup ---

    void setPosition(const Vec3& pos);
    void setTarget(const Vec3& target);
    void setUp(const Vec3& up);

    // lookAt convenience
    void lookAt(const Vec3& pos, const Vec3& target, const Vec3& up = Vec3::up());

    // Perspective settings
    void setFovDeg(float fovDegrees);
    void setFovRad(float fovRadians);
    void setAspect(float aspect);                  // width / height
    void setAspect(int screenW, int screenH);
    void setNearFar(float zNear, float zFar);

    // --- Accessors (all lazy-computed) ---

    const Vec3& position() const { return position_; }
    const Vec3& target()   const { return target_; }
    const Vec3& up()       const { return up_; }

    // Individual matrices
    const Mat4& viewMatrix()     const;
    const Mat4& projMatrix()     const;
    // Combined VP — equivalent to PS2GL VertexXform (without GS scale)
    const Mat4& viewProjMatrix() const;

    // --- Ray generation for raycast ---
    // sx, sy in [-1, 1] NDC (0,0 = center; PS2: x right, y up)
    // Returns a ray in world space (origin + normalized direction)
    struct Ray {
        Vec3 origin;
        Vec3 direction;
    };
    Ray screenToWorldRay(float ndcX, float ndcY) const;

    // --- Frustum culling helpers ---
    bool isPointVisible(const Vec3& p)                   const;
    bool isSphereVisible(const Vec3& center, float r)    const;
    bool isBoxVisible(const Vec3& bMin, const Vec3& bMax) const;

private:
    void markDirty();
    void recalculate() const;

    Vec3  position_ = {0.0f, 0.0f, 5.0f};
    Vec3  target_   = {0.0f, 0.0f, 0.0f};
    Vec3  up_       = {0.0f, 1.0f, 0.0f};
    float fov_      = math::toRad(60.0f);
    float aspect_   = 640.0f / 448.0f;   // PS2 default
    float zNear_    = 0.1f;
    float zFar_     = 1000.0f;

    mutable bool dirty_    = true;
    mutable Mat4 view_;
    mutable Mat4 proj_;
    mutable Mat4 viewProj_;

    // Frustum planes (world space), recomputed with viewProj
    mutable Vec4 frustumPlanes_[6];
};

} // namespace xy
