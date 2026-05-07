#include "xy_light.hpp"
#include <cmath>

namespace xy {

XYLightSystem::XYLightSystem() {
    // Set reasonable defaults for light 0 (mirrors PS2GL Light0 special case)
    lights_[0].diffuse  = {1.0f, 1.0f, 1.0f};
    lights_[0].specular = {1.0f, 1.0f, 1.0f};
}

XYLight& XYLightSystem::light(int index) {
    return lights_[index];
}

const XYLight& XYLightSystem::light(int index) const {
    return lights_[index];
}

void XYLightSystem::enableLight(int index, LightType type) {
    lights_[index].enabled = true;
    lights_[index].type    = type;
}

void XYLightSystem::disableLight(int index) {
    lights_[index].enabled = false;
}

int XYLightSystem::numDirectional() const {
    int n = 0;
    for (int i = 0; i < MAX_LIGHTS; i++)
        if (lights_[i].enabled && lights_[i].type == LightType::Directional) n++;
    return n;
}

int XYLightSystem::numPoint() const {
    int n = 0;
    for (int i = 0; i < MAX_LIGHTS; i++)
        if (lights_[i].enabled && lights_[i].type == LightType::Point) n++;
    return n;
}

int XYLightSystem::numSpot() const {
    int n = 0;
    for (int i = 0; i < MAX_LIGHTS; i++)
        if (lights_[i].enabled && lights_[i].type == LightType::Spot) n++;
    return n;
}

// ---------------------------------------------------------------------------
// CPU Phong shading
//
// Mirrors the math done by PS2GL's VU1 microcode in:
//   EXTERNAL_PS2GL/vu1/general.vcl  (specular + all light types)
//   EXTERNAL_PS2GL/vu1/fast.vcl     (no specular, directional only)
//
// Key formulas from VU1 microcode comments / PS2GL base_renderer.cpp:
//   diffuse  = max(0, dot(N, L)) * light.diffuse * mat.diffuse
//   specular = pow(max(0, dot(R, V)), shininess) * light.specular * mat.specular
//   attenuation = 1 / (c + l*d + q*d^2)   (point/spot only)
// ---------------------------------------------------------------------------

Color XYLightSystem::calcPhong(
    const Vec3& pos,
    const Vec3& normal,
    const Vec3& eyeDir,
    const Vec3& matAmbient,
    const Vec3& matDiffuse,
    const Vec3& matSpecular,
    float       shininess) const
{
    if (!enabled_) {
        // No lighting — return raw diffuse (like PS2GL with lighting disabled)
        return Color::fromFloat(matDiffuse.x, matDiffuse.y, matDiffuse.z);
    }

    Vec3 N = normal.normalized();

    // Start with global ambient contribution
    Vec3 result = {
        globalAmbient_.x * matAmbient.x,
        globalAmbient_.y * matAmbient.y,
        globalAmbient_.z * matAmbient.z
    };

    for (int i = 0; i < MAX_LIGHTS; i++) {
        const XYLight& light = lights_[i];
        if (!light.enabled) continue;

        // --- Light direction and attenuation ---
        Vec3  L;
        float atten = 1.0f;

        if (light.type == LightType::Directional) {
            // PS2GL: directional light direction is pre-normalized and stored
            // in Position with w=0 after SetDirection() transforms it
            L = (light.direction * -1.0f).normalized();
            // No attenuation for directional lights
        } else {
            Vec3  toLight = light.position - pos;
            float dist    = toLight.length();
            L = (dist > 1e-6f) ? toLight / dist : Vec3::up();

            // Attenuation (PS2GL base_renderer.cpp AddVu1RendererContext)
            float denom = light.constantAtten
                        + light.linearAtten   * dist
                        + light.quadraticAtten * dist * dist;
            atten = (denom > 1e-6f) ? 1.0f / denom : 0.0f;

            // Spotlight cone
            if (light.isSpot()) {
                float cosAngle  = L.dot((light.direction * -1.0f).normalized());
                float cosLimit  = cosf(math::toRad(light.spotCutoffDeg));
                if (cosAngle < cosLimit) {
                    atten = 0.0f;
                } else {
                    atten *= powf(cosAngle, light.spotExponent);
                }
            }
        }

        // --- Ambient contribution ---
        result.x += light.ambient.x * matAmbient.x * atten;
        result.y += light.ambient.y * matAmbient.y * atten;
        result.z += light.ambient.z * matAmbient.z * atten;

        // --- Diffuse (Lambertian) ---
        float NdotL = N.dot(L);
        if (NdotL > 0.0f) {
            float d = NdotL * atten;
            result.x += light.diffuse.x * matDiffuse.x * d;
            result.y += light.diffuse.y * matDiffuse.y * d;
            result.z += light.diffuse.z * matDiffuse.z * d;

            // --- Specular (Blinn-Phong half vector) ---
            if (shininess > 0.0f &&
                (light.specular.x > 0 || light.specular.y > 0 || light.specular.z > 0)) {
                Vec3  H     = (L + eyeDir).normalized();
                float NdotH = N.dot(H);
                if (NdotH > 0.0f) {
                    float spec = powf(NdotH, shininess) * atten;
                    result.x += light.specular.x * matSpecular.x * spec;
                    result.y += light.specular.y * matSpecular.y * spec;
                    result.z += light.specular.z * matSpecular.z * spec;
                }
            }
        }
    }

    // Clamp to [0, 1]
    result.x = math::clamp(result.x, 0.0f, 1.0f);
    result.y = math::clamp(result.y, 0.0f, 1.0f);
    result.z = math::clamp(result.z, 0.0f, 1.0f);

    return Color::fromFloat(result.x, result.y, result.z);
}

} // namespace xy
