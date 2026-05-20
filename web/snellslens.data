#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Output fragment color
out vec4 finalColor;

uniform vec2 mouse;
uniform vec2 offset;
uniform vec2 textureSize;
uniform float radius;
uniform float ior;
uniform float scale;
uniform sampler2D texture0;

float hemisphereZ(float x, float y) {
  return sqrt(radius * radius - x * x - y * y);
}

float hemisphereSurfacePointX(float x, float y) {
  return -x / sqrt(radius * radius - x * x - y * y);
}

float hemisphereSurfacePointY(float x, float y) {
  return -y / sqrt(radius * radius - x * x - y * y);
}

void main() {
  vec2 pixelCoord = fragTexCoord * textureSize;
  vec2 mouseCoord = (mouse - offset) / scale;
  vec2 sampleCoord = pixelCoord;

  if (distance(pixelCoord, mouseCoord) < radius) {
    float x = pixelCoord.x - mouseCoord.x;
    float y = pixelCoord.y - mouseCoord.y;
    float z = hemisphereZ(x, y);
    float mu = 1.0 / ior;

    vec3 i = vec3(0.0, 0.0, 1.0);
    vec3 n = normalize(vec3(
      hemisphereSurfacePointX(x, y),
      hemisphereSurfacePointY(x, y),
      1.0
    ));

    float c = dot(n, i);
    vec3 tr = sqrt(1.0 - mu * mu * (1.0 - c * c)) * n + mu * (i - c * n);
    // vec3 tr = refract(i, n, mu);

    float t = -z / tr.z;

    sampleCoord = pixelCoord - tr.xy * t;
  }

  vec2 uv = sampleCoord / textureSize;
  finalColor = texture(texture0, clamp(uv, vec2(0.0), vec2(1.0)));
}
