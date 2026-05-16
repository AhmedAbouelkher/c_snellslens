#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

#include <math.h>
#include <raylib.h>
#include <stdio.h>

float vDot3(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

float vLength3(Vector3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

Vector3 vNormalize3(Vector3 v) {
  float length = vLength3(v);
  if (length == 0.0f) {
    return (Vector3){0, 0, 0};
  }
  return (Vector3){v.x / length, v.y / length, v.z / length};
}

Vector3 vMulByScalar3(Vector3 a, float scalar) {
  return (Vector3){a.x * scalar, a.y * scalar, a.z * scalar};
}

Vector3 vAddVectors3(Vector3 a, Vector3 b) {
  return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 vSubVectors3(Vector3 a, Vector3 b) {
  return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 vSqrt3(Vector3 a) {
  return (Vector3){a.x < 0 ? 0 : sqrtf(a.x), a.y < 0 ? 0 : sqrtf(a.y),
                   a.z < 0 ? 0 : sqrtf(a.z)};
}

float vDistance2(Vector2 a, Vector2 b) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return sqrtf(dx * dx + dy * dy);
}

// This function implements the refraction of a vector at the interface between
// two media using Snell's Law in vector form
// Copied from:
// https://registry.khronos.org/OpenGL-Refpages/gl4/html/refract.xhtml
Vector3 vRefract(Vector3 i, Vector3 n, float eta) {
  float cosi = -vDot3(n, i);
  float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
  if (k < 0.0f) {
    return (Vector3){0, 0, 0};
  }
  return vAddVectors3(vMulByScalar3(i, eta),
                      vMulByScalar3(n, eta * cosi - sqrtf(k)));
}

Vector3 vRefractShader(Vector3 i, Vector3 n, float mu) {
  float c = -vDot3(n, i);
  float k = 1.0f - mu * mu * (1.0f - c * c);
  if (k < 0.0f) {
    return (Vector3){0, 0, 0};
  }
  return vAddVectors3(vMulByScalar3(n, sqrtf(k)),
                      vMulByScalar3(vSubVectors3(i, vMulByScalar3(n, c)), mu));
}

void vPrint2(Vector2 a) { printf("(%.2f, %.2f)", a.x, a.y); }

void vPrint3(Vector3 a) { printf("(%.2f, %.2f, %.2f)", a.x, a.y, a.z); }

char *vToStr2(Vector2 a) {
  static char buffer[50];
  snprintf(buffer, sizeof(buffer), "(%.2f, %.2f)", a.x, a.y);
  return buffer;
}

char *vToStr3(Vector3 a) {
  static char buffer[100];
  snprintf(buffer, sizeof(buffer), "(%.2f, %.2f, %.2f)", a.x, a.y, a.z);
  return buffer;
}

#endif