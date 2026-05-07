#pragma once

// ---------------------------------------------------------------------------
// xy_primitives.hpp
//
// Factory functions for common 3D meshes.
// Returns heap-allocated XYMesh objects.
//
// Inspired by PS2GL examples that build geometry manually before calling
// glBegin/glEnd. XY makes this explicit and reusable.
// ---------------------------------------------------------------------------

#include "xy_mesh.hpp"
#include <cmath>

namespace xy {
namespace primitives {

// --- Cube ---
// 6 faces × 2 triangles × 3 vertices = 36 vertices
// Normals per face (flat-shaded), UVs mapped per face [0,1]
inline XYMesh* createCube(float halfExtent = 0.5f) {
    XYMesh* m = new XYMesh();
    float e = halfExtent;

    // Helper: add a quad (two triangles) as 4 vertices + 6 indices
    auto addFace = [&](
        const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3,
        const Vec3& n)
    {
        u32 base = (u32)m->vertexCount();
        m->addVertex(Vertex(p0, n, {0.0f, 0.0f}));
        m->addVertex(Vertex(p1, n, {1.0f, 0.0f}));
        m->addVertex(Vertex(p2, n, {1.0f, 1.0f}));
        m->addVertex(Vertex(p3, n, {0.0f, 1.0f}));
        m->addTriangle(base+0, base+1, base+2);
        m->addTriangle(base+0, base+2, base+3);
    };

    addFace({-e,-e, e}, { e,-e, e}, { e, e, e}, {-e, e, e}, { 0, 0, 1}); // +Z
    addFace({ e,-e,-e}, {-e,-e,-e}, {-e, e,-e}, { e, e,-e}, { 0, 0,-1}); // -Z
    addFace({-e, e,-e}, {-e, e, e}, { e, e, e}, { e, e,-e}, { 0, 1, 0}); // +Y
    addFace({-e,-e, e}, {-e,-e,-e}, { e,-e,-e}, { e,-e, e}, { 0,-1, 0}); // -Y
    addFace({ e,-e, e}, { e,-e,-e}, { e, e,-e}, { e, e, e}, { 1, 0, 0}); // +X
    addFace({-e,-e,-e}, {-e,-e, e}, {-e, e, e}, {-e, e,-e}, {-1, 0, 0}); // -X

    m->calcBounds();
    return m;
}

// --- Plane ---
// Single quad facing +Y, subdivided into segW × segH quads
inline XYMesh* createPlane(float width = 1.0f, float depth = 1.0f,
                            int segW = 1, int segH = 1) {
    XYMesh* m = new XYMesh();
    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    Vec3 normal = {0.0f, 1.0f, 0.0f};

    for (int j = 0; j <= segH; j++) {
        for (int i = 0; i <= segW; i++) {
            float tx = (float)i / segW;
            float tz = (float)j / segH;
            Vec3 pos  = { -hw + tx * width, 0.0f, -hd + tz * depth };
            m->addVertex(Vertex(pos, normal, {tx, tz}));
        }
    }

    int stride = segW + 1;
    for (int j = 0; j < segH; j++) {
        for (int i = 0; i < segW; i++) {
            u32 tl = (u32)(j * stride + i);
            u32 tr = tl + 1;
            u32 bl = tl + (u32)stride;
            u32 br = bl + 1;
            m->addTriangle(tl, bl, tr);
            m->addTriangle(tr, bl, br);
        }
    }

    m->calcBounds();
    return m;
}

// --- UV Sphere ---
// stacks × slices, smooth normals
inline XYMesh* createSphere(float radius = 0.5f, int stacks = 12, int slices = 16) {
    XYMesh* m = new XYMesh();

    for (int i = 0; i <= stacks; i++) {
        float phi    = math::PI * ((float)i / stacks);         // 0 .. PI
        float cosPhi = cosf(phi);
        float sinPhi = sinf(phi);
        float v      = (float)i / stacks;

        for (int j = 0; j <= slices; j++) {
            float theta    = math::TWO_PI * ((float)j / slices);  // 0 .. 2PI
            float cosTheta = cosf(theta);
            float sinTheta = sinf(theta);
            float u        = (float)j / slices;

            Vec3 n = { sinPhi * cosTheta, cosPhi, sinPhi * sinTheta };
            Vec3 p = { n.x * radius, n.y * radius, n.z * radius };
            m->addVertex(Vertex(p, n, {u, v}));
        }
    }

    int stride = slices + 1;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            u32 tl = (u32)(i * stride + j);
            u32 tr = tl + 1;
            u32 bl = tl + (u32)stride;
            u32 br = bl + 1;
            m->addTriangle(tl, tr, bl);
            m->addTriangle(tr, br, bl);
        }
    }

    m->calcBounds();
    return m;
}

// --- Cylinder ---
// Closed cylinder aligned on Y axis
inline XYMesh* createCylinder(float radius = 0.5f, float height = 1.0f, int slices = 16) {
    XYMesh* m = new XYMesh();
    float hh = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= slices; i++) {
        float theta    = math::TWO_PI * ((float)i / slices);
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);
        float u        = (float)i / slices;
        Vec3 n = {cosTheta, 0.0f, sinTheta};

        m->addVertex(Vertex({cosTheta*radius, -hh, sinTheta*radius}, n, {u, 0.0f}));
        m->addVertex(Vertex({cosTheta*radius,  hh, sinTheta*radius}, n, {u, 1.0f}));
    }

    int stride = 2;
    for (int i = 0; i < slices; i++) {
        u32 bl = (u32)(i * stride);
        u32 tl = bl + 1;
        u32 br = bl + (u32)stride;
        u32 tr = br + 1;
        m->addTriangle(bl, br, tl);
        m->addTriangle(tl, br, tr);
    }

    // Top cap — center vertex then fan
    u32 topCenter = (u32)m->vertexCount();
    m->addVertex(Vertex({0.0f, hh, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}));
    for (int i = 0; i < slices; i++) {
        float t0 = math::TWO_PI * ((float)i / slices);
        float t1 = math::TWO_PI * ((float)(i+1) / slices);
        u32 v0 = (u32)m->vertexCount();
        m->addVertex(Vertex({cosf(t0)*radius, hh, sinf(t0)*radius}, {0.0f, 1.0f, 0.0f},
                            {0.5f + cosf(t0)*0.5f, 0.5f + sinf(t0)*0.5f}));
        m->addVertex(Vertex({cosf(t1)*radius, hh, sinf(t1)*radius}, {0.0f, 1.0f, 0.0f},
                            {0.5f + cosf(t1)*0.5f, 0.5f + sinf(t1)*0.5f}));
        m->addTriangle(topCenter, v0, v0+1);
    }

    // Bottom cap
    u32 botCenter = (u32)m->vertexCount();
    m->addVertex(Vertex({0.0f, -hh, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}));
    for (int i = 0; i < slices; i++) {
        float t0 = math::TWO_PI * ((float)i / slices);
        float t1 = math::TWO_PI * ((float)(i+1) / slices);
        u32 v0 = (u32)m->vertexCount();
        m->addVertex(Vertex({cosf(t0)*radius,-hh, sinf(t0)*radius}, {0.0f,-1.0f, 0.0f},
                            {0.5f + cosf(t0)*0.5f, 0.5f + sinf(t0)*0.5f}));
        m->addVertex(Vertex({cosf(t1)*radius,-hh, sinf(t1)*radius}, {0.0f,-1.0f, 0.0f},
                            {0.5f + cosf(t1)*0.5f, 0.5f + sinf(t1)*0.5f}));
        m->addTriangle(botCenter, v0+1, v0);
    }

    m->calcBounds();
    return m;
}

} // namespace primitives
} // namespace xy
