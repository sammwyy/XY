#pragma once

#include "xy_math.hpp"

namespace xy {

// ---------------------------------------------------------------------------
// Light types — mirrors PS2GL's tLightType enum
// ---------------------------------------------------------------------------

enum class LightType {
    Directional,  // Infinite, no position, w=0 in PS2GL
    Point,        // Position in world space, with attenuation
    Spot,         // Point + cone (cutoff + exponent)
};

// ---------------------------------------------------------------------------
// XYLight
//
// Direct port of CImmLight's data layout (lighting.h / lighting.cpp).
//
// PS2GL packs per-light data as 6 qwords in VU1 memory (kLightStructSize=6):
//   [0] ambient   [1] diffuse   [2] specular
//   [3] position  [4] spotDir   [5] attenCoeffs
//
// We keep the same fields so a future VU1 path can pack them directly.
//
// Key insight from PS2GL lighting.cpp SetPosition():
//   Position is stored PRE-TRANSFORMED by the current ModelView.
//   This avoids re-transforming per vertex on VU1.
//   XYLightSystem::prepareForMesh() does the same pre-transform.
// ---------------------------------------------------------------------------

struct XYLight {
    LightType type    = LightType::Directional;
    bool      enabled = false;

    // Colors (normalized 0..1, like PS2GL's cpu_vec_xyzw)
    Vec3 ambient  = {0.0f, 0.0f, 0.0f};
    Vec3 diffuse  = {1.0f, 1.0f, 1.0f};
    Vec3 specular = {0.0f, 0.0f, 0.0f};

    // World-space position (Point / Spot)
    Vec3 position  = {0.0f, 5.0f, 0.0f};
    // World-space direction (Directional: toward light; Spot: spotlight axis)
    Vec3 direction = {0.0f, -1.0f, 0.0f};

    // Spotlight parameters (PS2GL: SpotCutoff==180 means Point)
    float spotCutoffDeg  = 180.0f;  // 180 = point light
    float spotExponent   = 1.0f;

    // Attenuation: 1 / (c + l*d + q*d^2)
    // PS2GL scales l and q by 1/normalScale when model matrix has uniform scale
    float constantAtten  = 1.0f;
    float linearAtten    = 0.0f;
    float quadraticAtten = 0.0f;

    bool isSpot() const { return type == LightType::Spot || spotCutoffDeg < 180.0f; }
};

// ---------------------------------------------------------------------------
// XYLightSystem
//
// Manages up to MAX_LIGHTS lights + global ambient.
// Also provides CPU Phong shading (used when VU1 path is not active).
//
// PS2GL equivalent: CLighting / CImmLighting
// ---------------------------------------------------------------------------

class XYLightSystem {
public:
    static const int MAX_LIGHTS = 8;   // PS2GL uses 8

    XYLightSystem();

    // --- Light management ---

    XYLight& light(int index);
    const XYLight& light(int index) const;
    void enableLight(int index,  LightType type = LightType::Directional);
    void disableLight(int index);

    // Global ambient (PS2GL: GlobalAmbient in CImmLighting)
    void       setGlobalAmbient(const Vec3& amb) { globalAmbient_ = amb; }
    const Vec3& globalAmbient() const { return globalAmbient_; }

    // --- CPU Phong shading ---
    // Computes final vertex color using Phong model.
    // eyeDir: normalized vector from vertex to camera (in world/object space).
    // normal must already be in the same space as lights' positions.
    //
    // This is the CPU fallback; the VU1 path uses the same math in microcode
    // (see fast.vcl / general.vcl in EXTERNAL_PS2GL/vu1/).
    Color calcPhong(const Vec3& pos,
                    const Vec3& normal,
                    const Vec3& eyeDir,
                    const Vec3& matAmbient,
                    const Vec3& matDiffuse,
                    const Vec3& matSpecular,
                    float       shininess) const;

    // Counts of active lights per type (used to select VU1 microcode variant)
    int numDirectional() const;
    int numPoint()       const;
    int numSpot()        const;
    bool lightingEnabled() const { return enabled_; }
    void setEnabled(bool e)      { enabled_ = e; }

private:
    XYLight lights_[MAX_LIGHTS];
    Vec3    globalAmbient_ = {0.1f, 0.1f, 0.1f};
    bool    enabled_       = true;
};

} // namespace xy
