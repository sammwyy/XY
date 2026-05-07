#pragma once

#include "xy_math.hpp"

namespace xy {

// Forward declare texture
class XYTexture;

// ---------------------------------------------------------------------------
// XYMaterial
//
// Mirrors PS2GL's CMaterial (ambient, diffuse, specular, emission, shininess)
// plus optional texture binding and per-vertex color mode.
//
// From PS2GL's material.cpp insight:
//  - useVertexColor maps to GL_COLOR_MATERIAL (GL_AMBIENT_AND_DIFFUSE)
//  - specular is only computed if lights have non-zero specular AND
//    material shininess > 0 (LightsHaveSpecular / MaterialHasSpecular)
// ---------------------------------------------------------------------------

struct XYMaterial {
    // Phong components (normalized [0,1] floats like PS2GL's cpu_vec_xyzw)
    Vec3  ambient   = {0.2f, 0.2f, 0.2f};
    Vec3  diffuse   = {0.8f, 0.8f, 0.8f};
    Vec3  specular  = {0.0f, 0.0f, 0.0f};
    Vec3  emission  = {0.0f, 0.0f, 0.0f};
    float shininess = 0.0f;   // 0 = no specular highlight
    float alpha     = 1.0f;   // overall opacity

    // Texture (optional)
    XYTexture* texture = nullptr;

    // When true, per-vertex color overrides diffuse (GL_COLOR_MATERIAL style)
    bool useVertexColor = false;

    // Culling
    bool doubleSided = false;

    // Quick check: does this material need specular pass?
    bool hasSpecular() const { return shininess > 0.0f &&
                               (specular.x > 0 || specular.y > 0 || specular.z > 0); }

    // --- Preset factories ---

    static XYMaterial flat(float r, float g, float b, float a = 1.0f) {
        XYMaterial m;
        m.diffuse  = {r, g, b};
        m.ambient  = {r * 0.25f, g * 0.25f, b * 0.25f};
        m.alpha    = a;
        return m;
    }

    static XYMaterial unlit() {
        XYMaterial m;
        m.useVertexColor = true;
        return m;
    }
};

} // namespace xy
