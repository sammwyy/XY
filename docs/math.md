# Math Library

The XY math library provides essential types for 3D graphics, physics, and general geometry on the PS2.

## Core Types

| Type | Description |
|---|---|
| `Vec2` | 2D vector (x, y). Used for UI and textures. |
| `Vec3` | 3D vector (x, y, z). Primary type for positions, normals, and directions. |
| `Vec4` | 4D vector (x, y, z, w). Used for homogeneous coordinates and plane equations. |
| `Mat4` | 4 × 4 matrix (row-major). Used for transformations (MVP, world). |
| `Color` | RGBA color (u8). Memory efficient [0, 255] range. |

## Memory Layout

The `Mat4` class uses **row-major** ordering. This matches the CPU-side matrix conventions in `PS2GL` and simplifies integration with the GS (Graphics Synthesizer) which expects data in specific orders.

```cpp
struct Mat4 {
    float m[4][4]; // [row][col]
};
```

## Vector Operations

Standard operators are overloaded for `Vec2`, `Vec3`, and `Vec4`:
- Arithmetic: `+`, `-`, `*` (scalar), `/` (scalar).
- Compound: `+=`, `-=`, `*=`.

### Vec3 Specifics
- `dot(o)`: Dot product.
- `cross(o)`: Cross product.
- `length()` / `lengthSq()`: Euclidean length.
- `normalized()`: Returns unit vector.

## Matrix Transformations

`Mat4` includes static factory methods for common transforms:

```cpp
// Translation, Rotation, Scale
Mat4 t = Mat4::translate({x, y, z});
Mat4 r = Mat4::rotateY(angleRad);
Mat4 s = Mat4::scale({sx, sy, sz});

// View & Projection
Mat4 view = Mat4::lookAt(eye, target, up);
Mat4 proj = Mat4::perspective(fovRad, aspect, near, far);
```

### Transformation API
- `transformPoint(v)`: Multiplies by matrix with $w=1$, includes translation.
- `transformDirection(v)`: Multiplies by matrix with $w=0$, ignores translation.
- `normalMatrix()`: Returns the **inverse-transpose** of the upper-left 3x3. Essential for transforming lighting normals correctly in the presence of non-uniform scaling.
- `inversedAffine()`: Fast inverse for matrices consisting only of rotation and translation.

## Colors

The `Color` struct stores 32-bit RGBA (8 bits per channel).

```cpp
// Creation
xy::Color c(255, 128, 0, 255);
auto red = xy::Color::fromFloat(1.0f, 0.0f, 0.0f);

// Utilities
auto mixed = xy::Color::lerp(colorA, colorB, 0.5f);
```

## Math Utilities (`xy::math`)

Common constants and helper functions are located in the `xy::math` namespace:

| Function | Description |
|---|---|
| `PI`, `TWO_PI`, `HALF_PI` | Common constants. |
| `toRad(deg)`, `toDeg(rad)` | Angle conversions. |
| `clamp(v, lo, hi)` | Standard clamp. |
| `lerp(a, b, t)` | Linear interpolation. |
| `min2(a, b)`, `max2(a, b)` | Basic comparisons. |
