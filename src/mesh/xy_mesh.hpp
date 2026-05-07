#pragma once

#include "xy_math.hpp"
#include <vector>

namespace xy {

// ---------------------------------------------------------------------------
// Vertex — interleaved layout (matches what a future VU1 program expects)
// Based on PS2GL's CGeometryBlock separate arrays but kept interleaved
// for simpler CPU processing. VU1 upload will deinterleave as needed.
// ---------------------------------------------------------------------------

struct Vertex {
    Vec3  position;   // object-space position
    Vec3  normal;     // for lighting (inverse-transpose transform)
    Vec2  uv;         // texture coordinates
    Color color;      // per-vertex color (used when material.useVertexColor)

    Vertex() = default;
    Vertex(const Vec3& pos, const Vec3& norm, const Vec2& uv, const Color& col = Color())
        : position(pos), normal(norm), uv(uv), color(col) {}
    
    // Convenience: position + normal only (common for lit geometry)
    Vertex(const Vec3& pos, const Vec3& norm)
        : position(pos), normal(norm), uv(0.0f, 0.0f), color(Color()) {}
};

// ---------------------------------------------------------------------------
// XYMesh — triangle soup or indexed triangle list
// Mirrors PS2GL CGeometryBlock but without the GL prim-type complexity.
// ---------------------------------------------------------------------------

class XYMesh {
public:
    XYMesh()  = default;
    ~XYMesh() = default;

    // Non-copyable, moveable
    XYMesh(const XYMesh&)            = delete;
    XYMesh& operator=(const XYMesh&) = delete;
    XYMesh(XYMesh&&)                 = default;
    XYMesh& operator=(XYMesh&&)      = default;

    // --- Build API ---

    void clear();

    // Add a single vertex, returns its index
    u32 addVertex(const Vertex& v);

    // Add a triangle by indices (indexed mode)
    void addTriangle(u32 i0, u32 i1, u32 i2);

    // Push a full triangle directly (non-indexed)
    void addTriangleDirect(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    // Recalculate flat normals from triangle geometry
    void calcFlatNormals();

    // Recalculate smooth (averaged) normals
    void calcSmoothNormals();

    // Recalculate the AABB bounds
    void calcBounds();

    // --- Query ---

    bool isIndexed()  const { return !indices_.empty(); }
    int  vertexCount() const { return (int)vertices_.size(); }
    int  indexCount()  const { return (int)indices_.size(); }
    int  triangleCount() const {
        return isIndexed() ? (int)(indices_.size() / 3)
                           : (int)(vertices_.size() / 3);
    }

    const Vertex* vertices() const { return vertices_.data(); }
    const u32*    indices()  const { return indices_.data(); }
    Vertex*       vertices()       { return vertices_.data(); }

    // Bounds (call calcBounds() first)
    const Vec3& boundsMin() const { return boundsMin_; }
    const Vec3& boundsMax() const { return boundsMax_; }
    Vec3        boundsCenter() const { return (boundsMin_ + boundsMax_) * 0.5f; }

private:
    std::vector<Vertex> vertices_;
    std::vector<u32>    indices_;
    Vec3                boundsMin_ = Vec3::zero();
    Vec3                boundsMax_ = Vec3::zero();
};

} // namespace xy
