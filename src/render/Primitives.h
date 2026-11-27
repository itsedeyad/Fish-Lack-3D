// =============================================================================
//  Primitives.h - reusable geometric building blocks with correct normals and
//  texture coordinates. Every model in the game (pier, trees, fish, rod, ...)
//  is assembled from these primitives plus glTranslate/glRotate/glScale.
// =============================================================================
#pragma once
#include "core/MathUtil.h"

namespace prim {

using lf::Vec3;
using lf::Color;

void color(const Color& c);
void color(const Color& c, float alpha);
/// Specular highlight colour/strength and shininess for the next draws.
void specular(float strength, float shininess);

/// Axis aligned box centred at the origin. texScale = texture repeats per metre.
void box(float sx, float sy, float sz, float texScale = 1.0f);
/// Box between two corners.
void boxFromTo(const Vec3& a, const Vec3& b, float texScale = 1.0f);
/// (Tapered) cylinder along +Y from y=0 to y=h. r1 = 0 gives a cone.
void cylinder(float r0, float r1, float h, int slices = 12, bool caps = true);
/// (Tapered) cylinder along +Z from z=0 to z=len.
void cylinderZ(float r0, float r1, float len, int slices = 10, bool caps = true);
/// Cylinder connecting two arbitrary points.
void cylinderBetween(const Vec3& a, const Vec3& b, float r0, float r1, int slices = 10);
void sphere(float r, int slices = 16, int stacks = 12);
/// Upper (top=true) or lower hemisphere.
void hemisphere(float r, bool top, int slices = 16, int stacks = 6);
/// Flat disc on the XZ plane facing +Y.
void disc(float r, int slices = 24);
/// Torus lying in the XY plane (axis = Z).
void torus(float innerR, float outerR, int sides = 8, int rings = 16);
/// Soft round "blob" shadow lying on the ground (alpha-blended, fades out).
void blobShadow(float cx, float cy, float cz, float rx, float rz, float alpha);

} // namespace prim
