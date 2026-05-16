#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#include "vector_math.h"

float global_lensRadius = 100.0f;
float global_lensIOR = 1.5f;
bool global_useShaderMode = false;
bool global_isLensEnabled = false;
bool global_readyToReset = false;

float hemisphereZ(float x, float y, float radius) {
  return sqrtf(radius * radius - x * x - y * y);
}

float hemisphereSurfacePointX(float x, float y, float radius) {
  return -x / sqrtf(radius * radius - x * x - y * y);
}

float hemisphereSurfacePointY(float x, float y, float radius) {
  return -y / sqrtf(radius * radius - x * x - y * y);
}

Vector3 vRefractShader(Vector3 i, Vector3 n, float mu) {
  float c = vDot2(n, i);
  float k = 1.0f - mu * mu * (1.0f - c * c);
  if (k < 0.0f) {
    return (Vector3){0, 0, 0};
  }
  return vAddVectors3(vMulByScalar2(n, sqrtf(k)),
                      vMulByScalar2(vSubVectors2(i, vMulByScalar2(n, c)), mu));
}

void applyLensOn(Image *dst, Image *src, int offsetX, int offsetY) {
  Vector2 nMousePosition = GetMousePosition();
  Vector2 lensCenter = {nMousePosition.x - offsetX, nMousePosition.y - offsetY};
  Color *srcPixels = src->data;
  Color *dstPixels = dst->data;
  int width = src->width < dst->width ? src->width : dst->width;
  int height = src->height < dst->height ? src->height : dst->height;
  int srcCount = src->width * src->height;
  int dstCount = dst->width * dst->height;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Vector2 pixelCoord = {(float)x, (float)y};
      float distance = vDistance2(pixelCoord, lensCenter);
      int dstIndex = y * dst->width + x;

      if (distance <= global_lensRadius) {
        float mu = 1.0f / global_lensIOR;

        float xx = pixelCoord.x - lensCenter.x;
        float yy = pixelCoord.y - lensCenter.y;
        float zz = hemisphereZ(xx, yy, global_lensRadius);

        Vector3 surfacePoint = {xx, yy, zz};
        Vector3 nNormal = vNormalize(surfacePoint);

        Vector3 iLightDirection = {0, 0, -1};
        Vector3 tr = vRefractShader(iLightDirection, nNormal, mu);
        if (tr.x == 0.0f && tr.y == 0.0f && tr.z == 0.0f) {
          continue;
        }

        float t = (-zz / tr.z);

        // <x, y, z> + t * tr
        int srcX = (int)(pixelCoord.x + tr.x * t);
        int srcY = (int)(pixelCoord.y + tr.y * t);

        int sampleIndex = srcY * src->width + srcX;
        if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height &&
            sampleIndex >= 0 && sampleIndex < srcCount) {
          dstPixels[dstIndex] = srcPixels[sampleIndex];
        }

        bool showDebug = IsKeyPressed(KEY_D);
        if (showDebug) {
          printf("coord: ");
          vPrint2(pixelCoord);
          printf(" mouse: ");
          vPrint2(lensCenter);
          printf(" xx: %f, yy: %f, zz: %f\n", xx, yy, zz);
          printf(" n: ");
          vPrint3(nNormal);
          printf(" i: ");
          vPrint3(iLightDirection);
          printf(" tr: ");
          vPrint3(tr);
          printf("\nt: %f, srcX: %d, srcY: %d\n", t, srcX, srcY);
          printf("-*-----\n");
        }
      }
    }
  }
}

int main() {
  InitWindow(1200, 900, "Snell's Lens");
  SetTargetFPS(60);

  Image sourceImage = LoadImage("assets/baboon.png");
  ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  Texture2D texture = LoadTextureFromImage(sourceImage);

  Shader lensShader = LoadShader(0, "lens_shader.glsl");
  int mouseLoc = GetShaderLocation(lensShader, "mouse");
  int radiusLoc = GetShaderLocation(lensShader, "radius");
  int iorLoc = GetShaderLocation(lensShader, "ior");
  int offsetLoc = GetShaderLocation(lensShader, "offset");
  int textureSizeLoc = GetShaderLocation(lensShader, "textureSize");

  while (!WindowShouldClose()) {
    if (IsFileDropped()) {
      FilePathList droppedFiles = LoadDroppedFiles();
      // only take the first file
      if (droppedFiles.count > 0) {
        UnloadImage(sourceImage);
        sourceImage = LoadImage(droppedFiles.paths[0]);
        ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        UnloadTexture(texture);
        texture = LoadTextureFromImage(sourceImage);
      }
      UnloadDroppedFiles(droppedFiles);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      global_isLensEnabled = !global_isLensEnabled;
      global_readyToReset = true;
    }

    if (IsKeyPressed(KEY_SPACE)) {
      global_useShaderMode = !global_useShaderMode;
      global_readyToReset = true;
    }

    if (IsKeyDown(KEY_W)) {
      global_lensRadius += 10.0f;
      if (global_lensRadius > 200.0f) {
        global_lensRadius = 200.0f;
      }

    } else if (IsKeyDown(KEY_S)) {
      global_lensRadius -= 10.0f;
      if (global_lensRadius < 30.0f) {
        global_lensRadius = 30.0f;
      }
    }

    if (IsKeyDown(KEY_I)) {
      global_lensIOR += 0.1f;
      if (global_lensIOR > 6.0f) {
        global_lensIOR = 6.0f;
      }
    } else if (IsKeyDown(KEY_O)) {
      global_lensIOR -= 0.1f;
      if (global_lensIOR < 1.0f) {
        global_lensIOR = 1.0f;
      }
    }

    if (global_isLensEnabled) {
      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    } else {
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    BeginDrawing();
    ClearBackground(BLACK);

    if (IsTextureValid(texture)) {
      int textureX = (GetScreenWidth() - sourceImage.width) / 2;
      int textureY = (GetScreenHeight() - sourceImage.height) / 2;

      if (global_isLensEnabled && global_useShaderMode) {
        UpdateTexture(texture, sourceImage.data);
        global_readyToReset = false;

        float mousePos[2] = {GetMousePosition().x, GetMousePosition().y};
        float lensRadius[1] = {global_lensRadius};
        float lensIOR[1] = {global_lensIOR};
        float offset[2] = {(float)textureX, (float)textureY};
        float textureSize[2] = {(float)sourceImage.width,
                                (float)sourceImage.height};

        SetShaderValue(lensShader, mouseLoc, mousePos, SHADER_UNIFORM_VEC2);
        SetShaderValue(lensShader, radiusLoc, lensRadius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lensShader, iorLoc, lensIOR, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lensShader, offsetLoc, offset, SHADER_UNIFORM_VEC2);
        SetShaderValue(lensShader, textureSizeLoc, textureSize,
                       SHADER_UNIFORM_VEC2);

        BeginShaderMode(lensShader);
        DrawTexture(texture, textureX, textureY, WHITE);
        EndShaderMode();

      } else if (global_isLensEnabled) {
        Image frame = ImageCopy(sourceImage);

        applyLensOn(&frame, &sourceImage, textureX, textureY);
        UpdateTexture(texture, frame.data);
        UnloadImage(frame);
      } else {
        if (global_readyToReset) {
          UpdateTexture(texture, sourceImage.data);
          global_readyToReset = false;
        }
      }

      if (!(global_isLensEnabled && global_useShaderMode)) {
        DrawTexture(texture, textureX, textureY, WHITE);
      }

      DrawFPS(10, 10);
      DrawText(TextFormat("Lens Radius: %.1f", global_lensRadius), 10, 30, 15,
               GREEN);
      DrawText(TextFormat("Lens IOR: %.1f", global_lensIOR), 10, 50, 15, GREEN);
      const float modeY = 65;
      if (global_useShaderMode) {
        DrawText("Shader Mode (GPU)", 10, modeY, 15, RED);
      } else {
        DrawText("CPU Mode", 10, modeY, 15, RED);
      }
      DrawText("[W] Increase Radius\n[S] Decrease Radius\n[I] Increase IOR"
               "\n[O] Decrease IOR\n[Right Click] Toggle Lens\n[Space] Toggle "
               "Shader/CPU",
               10, 80, 15, GREEN);

    } else {
      const char *text =
          "Drag and drop an image to see the Snell's Lens effect";
      int textWidth = MeasureText(text, 20);
      int textHeight = 20;
      DrawText(text, (GetScreenWidth() - textWidth) / 2,
               (GetScreenHeight() - textHeight) / 2, textHeight, WHITE);
    }

    EndDrawing();
  }

  UnloadImage(sourceImage);
  UnloadTexture(texture);
  UnloadShader(lensShader);
  CloseWindow();
  return 0;
}
