#include "xy_mesh.hpp"
#include <cstring>

namespace xy {

void XYMesh::clear() {
    vertices_.clear();
    indices_.clear();
    boundsMin_ = Vec3::zero();
    boundsMax_ = Vec3::zero();
}

u32 XYMesh::addVertex(const Vertex& v) {
    u32 idx = (u32)vertices_.size();
    vertices_.push_back(v);
    return idx;
}

void XYMesh::addTriangle(u32 i0, u32 i1, u32 i2) {
    indices_.push_back(i0);
    indices_.push_back(i1);
    indices_.push_back(i2);
}

void XYMesh::addTriangleDirect(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    u32 base = (u32)vertices_.size();
    vertices_.push_back(v0);
    vertices_.push_back(v1);
    vertices_.push_back(v2);
    indices_.push_back(base);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
}

void XYMesh::calcFlatNormals() {
    // Zero out normals first
    for (auto& v : vertices_) v.normal = Vec3::zero();

    int triCount = triangleCount();
    for (int t = 0; t < triCount; ++t) {
        u32 i0, i1, i2;
        if (isIndexed()) {
            i0 = indices_[t * 3 + 0];
            i1 = indices_[t * 3 + 1];
            i2 = indices_[t * 3 + 2];
        } else {
            i0 = t * 3 + 0;
            i1 = t * 3 + 1;
            i2 = t * 3 + 2;
        }

        const Vec3& p0 = vertices_[i0].position;
        const Vec3& p1 = vertices_[i1].position;
        const Vec3& p2 = vertices_[i2].position;

        Vec3 n = (p1 - p0).cross(p2 - p0).normalized();
        vertices_[i0].normal = n;
        vertices_[i1].normal = n;
        vertices_[i2].normal = n;
    }
}

void XYMesh::calcSmoothNormals() {
    // PS2GL note: smooth normals require averaging per-vertex contributions
    // from all triangles sharing that vertex. Same technique used before
    // packing data for VU1 upload.

    for (auto& v : vertices_) v.normal = Vec3::zero();

    int triCount = triangleCount();
    for (int t = 0; t < triCount; ++t) {
        u32 i0, i1, i2;
        if (isIndexed()) {
            i0 = indices_[t * 3 + 0];
            i1 = indices_[t * 3 + 1];
            i2 = indices_[t * 3 + 2];
        } else {
            i0 = t * 3 + 0;
            i1 = t * 3 + 1;
            i2 = t * 3 + 2;
        }

        const Vec3& p0 = vertices_[i0].position;
        const Vec3& p1 = vertices_[i1].position;
        const Vec3& p2 = vertices_[i2].position;

        Vec3 n = (p1 - p0).cross(p2 - p0); // not normalized — weight by area
        vertices_[i0].normal += n;
        vertices_[i1].normal += n;
        vertices_[i2].normal += n;
    }

    for (auto& v : vertices_) v.normal = v.normal.normalized();
}

void XYMesh::calcBounds() {
    if (vertices_.empty()) {
        boundsMin_ = boundsMax_ = Vec3::zero();
        return;
    }

    boundsMin_ = boundsMax_ = vertices_[0].position;
    for (const auto& v : vertices_) {
        const Vec3& p = v.position;
        if (p.x < boundsMin_.x) boundsMin_.x = p.x;
        if (p.y < boundsMin_.y) boundsMin_.y = p.y;
        if (p.z < boundsMin_.z) boundsMin_.z = p.z;
        if (p.x > boundsMax_.x) boundsMax_.x = p.x;
        if (p.y > boundsMax_.y) boundsMax_.y = p.y;
        if (p.z > boundsMax_.z) boundsMax_.z = p.z;
    }
}

} // namespace xy
