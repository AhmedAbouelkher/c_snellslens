#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#include "vector_math.h"

float global_lensRadius = 100.0f;
float global_lensIOR = 1.52f;
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

void applyLensOn(Image *dst, Image *src, int offsetX, int offsetY,
                 float scale) {
  Vector2 nMousePosition = GetMousePosition();
  Vector2 lensCenter = {(nMousePosition.x - offsetX) / scale,
                        (nMousePosition.y - offsetY) / scale};
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
        Vector3 nNormal = vNormalize3(surfacePoint);

        Vector3 iLightDirection = {0, 0, -1};
        Vector3 tr = vRefract(iLightDirection, nNormal, mu);
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
          printf(" tr: n");
          vPrint3(tr);
          printf("\nt: %f, srcX: %d, srcY: %d\n", t, srcX, srcY);
          printf("-*-----\n");
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  char *imagePath = NULL;
  if (argc > 1) {
    imagePath = argv[1];
  }

  InitWindow(1200, 900, "Snell's Lens");
  SetTargetFPS(60);

  Image sourceImage = {0};
  if (imagePath) {
    Image sourceImage = LoadImage("assets/baboon.png");
    ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  }
  Texture2D texture = LoadTextureFromImage(sourceImage);

  Shader lensShader = LoadShader(0, "lens_shader.glsl");
  int mouseLoc = GetShaderLocation(lensShader, "mouse");
  int radiusLoc = GetShaderLocation(lensShader, "radius");
  int iorLoc = GetShaderLocation(lensShader, "ior");
  int offsetLoc = GetShaderLocation(lensShader, "offset");
  int textureSizeLoc = GetShaderLocation(lensShader, "textureSize");
  int scaleLoc = GetShaderLocation(lensShader, "scale");

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
        global_isLensEnabled = true;
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
      if (global_lensRadius > 300.0f) {
        global_lensRadius = 300.0f;
      }

    } else if (IsKeyDown(KEY_S)) {
      global_lensRadius -= 10.0f;
      if (global_lensRadius < 30.0f) {
        global_lensRadius = 30.0f;
      }
    }

    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0.0f) {
      global_lensIOR += wheelMove * .1f;
      if (global_lensIOR > 7.0f) {
        global_lensIOR = 7.0f;
      }
      if (global_lensIOR < 1.25f) {
        global_lensIOR = 1.25f;
      }
    }

    if (IsKeyDown(KEY_I)) {
      global_lensIOR += 0.1f;
      if (global_lensIOR > 7.0f) {
        global_lensIOR = 7.0f;
      }
    } else if (IsKeyDown(KEY_O)) {
      global_lensIOR -= 0.1f;
      if (global_lensIOR < 1.25f) {
        global_lensIOR = 1.25f;
      }
    }

    if (global_isLensEnabled && IsTextureValid(texture)) {
      HideCursor();
    } else {
      ShowCursor();
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    BeginDrawing();
    ClearBackground(BLACK);

    if (IsTextureValid(texture)) {
      float scaleX = (float)GetScreenWidth() / (float)sourceImage.width;
      float scaleY = (float)GetScreenHeight() / (float)sourceImage.height;
      float scale = scaleX < scaleY ? scaleX : scaleY;
      float textureWidth = (float)sourceImage.width * scale;
      float textureHeight = (float)sourceImage.height * scale;
      Rectangle textureRect = {
          (GetScreenWidth() - textureWidth) / 2.0f,
          (GetScreenHeight() - textureHeight) / 2.0f,
          textureWidth,
          textureHeight,
      };

      if (global_isLensEnabled && global_useShaderMode) {
        UpdateTexture(texture, sourceImage.data);
        global_readyToReset = false;

        float mousePos[2] = {GetMousePosition().x, GetMousePosition().y};
        float lensRadius[1] = {global_lensRadius};
        float lensIOR[1] = {global_lensIOR};
        float offset[2] = {textureRect.x, textureRect.y};
        float imageScale[1] = {scale};
        float textureSize[2] = {(float)sourceImage.width,
                                (float)sourceImage.height};

        SetShaderValue(lensShader, mouseLoc, mousePos, SHADER_UNIFORM_VEC2);
        SetShaderValue(lensShader, radiusLoc, lensRadius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lensShader, iorLoc, lensIOR, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lensShader, offsetLoc, offset, SHADER_UNIFORM_VEC2);
        SetShaderValue(lensShader, textureSizeLoc, textureSize,
                       SHADER_UNIFORM_VEC2);
        SetShaderValue(lensShader, scaleLoc, imageScale, SHADER_UNIFORM_FLOAT);

        BeginShaderMode(lensShader);
        DrawTexturePro(texture,
                       CLITERAL(Rectangle){0.0f, 0.0f, (float)texture.width,
                                           (float)texture.height},
                       textureRect, CLITERAL(Vector2){0.0f, 0.0f}, 0.0f, WHITE);
        EndShaderMode();

      } else if (global_isLensEnabled) {
        Image frame = ImageCopy(sourceImage);

        applyLensOn(&frame, &sourceImage, (int)textureRect.x,
                    (int)textureRect.y, scale);
        UpdateTexture(texture, frame.data);
        UnloadImage(frame);
      } else {
        if (global_readyToReset) {
          UpdateTexture(texture, sourceImage.data);
          global_readyToReset = false;
        }
      }

      if (!(global_isLensEnabled && global_useShaderMode)) {
        DrawTexturePro(texture,
                       CLITERAL(Rectangle){0.0f, 0.0f, (float)texture.width,
                                           (float)texture.height},
                       textureRect, CLITERAL(Vector2){0.0f, 0.0f}, 0.0f, WHITE);
      }

      char *controlsText = "[Right Click] Toggle Lens\n[W] Increase "
                           "Radius\n[S] Decrease Radius\n[I] Increase IOR"
                           "\n[O] Decrease IOR\n[Mouse Wheel] Change "
                           "Radius\n[Space] Toggle Shader/CPU";
      int controlsTextWidth = MeasureText(controlsText, 15);

      char *shaderModeText =
          global_useShaderMode ? "GPU Shader Mode (Fast)" : "CPU Mode (Slow)";
      int shaderModeTextWidth = MeasureText(shaderModeText, 20);

      DrawRectangle(0, 0, controlsTextWidth * 1.1f + controlsTextWidth * .2f,
                    controlsTextWidth * 1.1f, CLITERAL(Color){0, 0, 0, 150});

      DrawFPS(10, 10);
      DrawText(
          TextFormat("R: %.1f | IOR: %.1f", global_lensRadius, global_lensIOR),
          10, 35, 15, WHITE);
      DrawText(controlsText, 10, 65, 15, WHITE);

      DrawText(shaderModeText, 10, 190, 20, global_useShaderMode ? GREEN : RED);

    } else {
      const char *text =
          "Drag and drop an image to see the Snell's Lens effect";
      int textHeight = 30;
      int textWidth = MeasureText(text, textHeight);
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
