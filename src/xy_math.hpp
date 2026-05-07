#pragma once

#include <tamtypes.h>
#include <cmath>

namespace xy {

// ---------------------------------------------------------------------------
// Vec2
// ---------------------------------------------------------------------------

struct Vec2 {
    float x, y;

    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2  operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2  operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2  operator*(float s)       const { return {x * s,   y * s};   }
};

// ---------------------------------------------------------------------------
// Vec3
// ---------------------------------------------------------------------------

struct Vec3 {
    float x, y, z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3  operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3  operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3  operator*(float s)       const { return {x * s,   y * s,   z * s};   }
    Vec3  operator/(float s)       const { return {x / s,   y / s,   z / s};   }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s)       { x *= s;   y *= s;   z *= s;   return *this; }

    float dot(const Vec3& o)   const { return x*o.x + y*o.y + z*o.z; }
    float lengthSq()           const { return x*x + y*y + z*z; }
    float length()             const { return sqrtf(lengthSq()); }

    Vec3 cross(const Vec3& o) const {
        return { y*o.z - z*o.y,
                 z*o.x - x*o.z,
                 x*o.y - y*o.x };
    }

    Vec3 normalized() const {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f, 0.0f};
        return *this / len;
    }

    static Vec3 zero()    { return {0.0f, 0.0f, 0.0f}; }
    static Vec3 one()     { return {1.0f, 1.0f, 1.0f}; }
    static Vec3 up()      { return {0.0f, 1.0f, 0.0f}; }
    static Vec3 forward() { return {0.0f, 0.0f,-1.0f}; }
    static Vec3 right()   { return {1.0f, 0.0f, 0.0f}; }

    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * t;
    }
};

// ---------------------------------------------------------------------------
// Vec4
// ---------------------------------------------------------------------------

struct Vec4 {
    float x, y, z, w;

    Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec3 xyz() const { return {x, y, z}; }
};

// ---------------------------------------------------------------------------
// Mat4  (row-major, same memory layout as PS2GL cpu_mat_44)
// m[row][col]
// ---------------------------------------------------------------------------

struct Mat4 {
    float m[4][4];

    Mat4() {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    // --- factories ---

    static Mat4 identity() { return Mat4{}; }

    static Mat4 translate(const Vec3& t) {
        Mat4 r;
        r.m[0][3] = t.x;
        r.m[1][3] = t.y;
        r.m[2][3] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        r.m[2][2] = s.z;
        return r;
    }

    static Mat4 rotateX(float rad) {
        Mat4 r;
        float c = cosf(rad), s = sinf(rad);
        r.m[1][1] =  c; r.m[1][2] = -s;
        r.m[2][1] =  s; r.m[2][2] =  c;
        return r;
    }

    static Mat4 rotateY(float rad) {
        Mat4 r;
        float c = cosf(rad), s = sinf(rad);
        r.m[0][0] =  c; r.m[0][2] =  s;
        r.m[2][0] = -s; r.m[2][2] =  c;
        return r;
    }

    static Mat4 rotateZ(float rad) {
        Mat4 r;
        float c = cosf(rad), s = sinf(rad);
        r.m[0][0] =  c; r.m[0][1] = -s;
        r.m[1][0] =  s; r.m[1][1] =  c;
        return r;
    }

    // Perspective projection (right-handed, maps Z to [-1, 1])
    static Mat4 perspective(float fovRad, float aspect, float zNear, float zFar) {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r.m[i][j] = 0.0f;
        float f = 1.0f / tanf(fovRad * 0.5f);
        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = (zFar + zNear) / (zNear - zFar);
        r.m[2][3] = (2.0f * zFar * zNear) / (zNear - zFar);
        r.m[3][2] = -1.0f;
        return r;
    }

    // LookAt view matrix (right-handed)
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 r = f.cross(up).normalized();
        Vec3 u = r.cross(f);
        Mat4 res;
        res.m[0][0] =  r.x; res.m[0][1] =  r.y; res.m[0][2] =  r.z; res.m[0][3] = -r.dot(eye);
        res.m[1][0] =  u.x; res.m[1][1] =  u.y; res.m[1][2] =  u.z; res.m[1][3] = -u.dot(eye);
        res.m[2][0] = -f.x; res.m[2][1] = -f.y; res.m[2][2] = -f.z; res.m[2][3] =  f.dot(eye);
        res.m[3][0] =  0.0f; res.m[3][1] = 0.0f; res.m[3][2] = 0.0f; res.m[3][3] = 1.0f;
        return res;
    }

    // --- operations ---

    Mat4 operator*(const Mat4& o) const {
        Mat4 res;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                res.m[i][j] = 0.0f;
                for (int k = 0; k < 4; k++)
                    res.m[i][j] += m[i][k] * o.m[k][j];
            }
        return res;
    }

    // Transform a point (w=1)
    Vec3 transformPoint(const Vec3& v) const {
        float x = m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3];
        float y = m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3];
        float z = m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3];
        float w = m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3];
        if (w != 0.0f && w != 1.0f) { x /= w; y /= w; z /= w; }
        return {x, y, z};
    }

    // Transform a direction (w=0, ignores translation)
    Vec3 transformDirection(const Vec3& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
        };
    }

    Vec4 transformVec4(const Vec4& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]*v.w,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]*v.w,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]*v.w,
            m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3]*v.w
        };
    }

    Mat4 transposed() const {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r.m[i][j] = m[j][i];
        return r;
    }

    // Inverse for affine matrices (rotation + translation only)
    // For full inverse use inversedGeneral()
    Mat4 inversedAffine() const {
        // Transpose rotation part, negate translation
        Mat4 r;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                r.m[i][j] = m[j][i];
        r.m[0][3] = -(r.m[0][0]*m[0][3] + r.m[0][1]*m[1][3] + r.m[0][2]*m[2][3]);
        r.m[1][3] = -(r.m[1][0]*m[0][3] + r.m[1][1]*m[1][3] + r.m[1][2]*m[2][3]);
        r.m[2][3] = -(r.m[2][0]*m[0][3] + r.m[2][1]*m[1][3] + r.m[2][2]*m[2][3]);
        r.m[3][0] = r.m[3][1] = r.m[3][2] = 0.0f;
        r.m[3][3] = 1.0f;
        return r;
    }

    // Normal matrix: inverse-transpose of the upper-left 3x3
    // (same approach as PS2GL's objToWorldXfrmTrans)
    Mat4 normalMatrix() const {
        Mat4 r = inversedAffine();
        return r.transposed();
    }

    const float* data() const { return &m[0][0]; }
};

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------

struct Color {
    u8 r, g, b, a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(u8 r, u8 g, u8 b, u8 a = 255) : r(r), g(g), b(b), a(a) {}

    // Blend two colors (lerp in u8 space)
    static Color lerp(const Color& a, const Color& b, float t) {
        return {
            (u8)(a.r + (b.r - a.r) * t),
            (u8)(a.g + (b.g - a.g) * t),
            (u8)(a.b + (b.b - a.b) * t),
            (u8)(a.a + (b.a - a.a) * t)
        };
    }

    // Convert from normalized [0,1] floats
    static Color fromFloat(float r, float g, float b, float a = 1.0f) {
        return {
            (u8)(r * 255.0f),
            (u8)(g * 255.0f),
            (u8)(b * 255.0f),
            (u8)(a * 255.0f)
        };
    }
};

// ---------------------------------------------------------------------------
// Math utilities
// ---------------------------------------------------------------------------

namespace math {

constexpr float PI      = 3.14159265358979323846f;
constexpr float TWO_PI  = 6.28318530717958647692f;
constexpr float HALF_PI = 1.57079632679489661923f;

inline float toRad(float deg) { return deg * (PI / 180.0f); }
inline float toDeg(float rad) { return rad * (180.0f / PI); }

template<typename T> inline T clamp(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
template<typename T> inline T min2(T a, T b) { return a < b ? a : b; }
template<typename T> inline T max2(T a, T b) { return a > b ? a : b; }
template<typename T> inline T lerp(T a, T b, float t) { return a + (b - a) * t; }

} // namespace math

} // namespace xy
