#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <string.h>

#if defined(_OPENMP)
#include <omp.h>
#endif /* defined(_OPENMP) */

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>

EM_JS(void, BrowserInitFileDropBridge, (void), {
  if (Module.raylibFileDropBridgeInstalled) {
    return;
  }

  Module.raylibFileDropBridgeInstalled = true;
  Module.raylibDroppedFiles = Module.raylibDroppedFiles || [];

  var canvas = Module.canvas;
  var fileInput = document.getElementById("raylib-file-input");

  if (!fileInput) {
    fileInput = document.createElement("input");
    fileInput.id = "raylib-file-input";
    fileInput.type = "file";
    fileInput.style.display = "none";
    document.body.appendChild(fileInput);
  }

  function queueFirstFile(file) {
    if (!file) {
      return;
    }

    var path = "/" + file.name;
    file.arrayBuffer().then(function(buffer) {
      if (Module.raylibDroppedFiles.length > 0) {
        return;
      }

      FS.writeFile(path, new Uint8Array(buffer));
      Module.raylibDroppedFiles.push(path);
    });
  }

  canvas.addEventListener("dragover", function(event) {
    event.preventDefault();
  }, false);

  canvas.addEventListener("drop", function(event) {
    event.preventDefault();

    var files = event.dataTransfer && event.dataTransfer.files;
    if (!files) {
      return;
    }

    queueFirstFile(files[0]);
  }, false);

  canvas.addEventListener("click", function() {
    fileInput.value = "";
    fileInput.click();
  }, false);

  fileInput.addEventListener("change", function(event) {
    var files = event.target.files;
    if (!files) {
      return;
    }

    queueFirstFile(files[0]);
  }, false);
});

EM_JS(int, BrowserDroppedFilesCount, (void), {
  return (Module.raylibDroppedFiles && Module.raylibDroppedFiles.length) ? Module.raylibDroppedFiles.length : 0;
});

EM_JS(void, BrowserDroppedFilePath, (int index, char *buffer, int bufferSize), {
  var path = (Module.raylibDroppedFiles && Module.raylibDroppedFiles[index]) ? Module.raylibDroppedFiles[index] : "";
  stringToUTF8(path, buffer, bufferSize);
});

EM_JS(void, BrowserClearDroppedFiles, (void), {
  if (Module.raylibDroppedFiles) {
    Module.raylibDroppedFiles.length = 0;
  }
});
#endif /* defined(PLATFORM_WEB) */

float global_lensRadius = 100.0f;
float global_lensIOR = 1.52f;
bool global_useShaderMode = true;
bool global_isLensEnabled = false;
bool global_readyToReset = false;
bool global_useParallel = true;

// This function implements the refraction of a vector at the interface between
// two media using Snell's Law in vector form
// Copied from:
// https://registry.khronos.org/OpenGL-Refpages/gl4/html/refract.xhtml
Vector3 vRefract(Vector3 i, Vector3 n, float eta) {
  float cosi = -Vector3DotProduct(n, i);
  float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
  if (k < 0.0f) {
    return (Vector3){0, 0, 0};
  }
  return Vector3Add(Vector3Scale(i, eta),
                    Vector3Scale(n, eta * cosi - sqrtf(k)));
}

Vector3 vRefractShader(Vector3 i, Vector3 n, float mu) {
  float c = -Vector3DotProduct(n, i);
  float k = 1.0f - mu * mu * (1.0f - c * c);
  if (k < 0.0f) {
    return (Vector3){0, 0, 0};
  }
  return Vector3Add(Vector3Scale(n, sqrtf(k)),
                    Vector3Scale(Vector3Subtract(i, Vector3Scale(n, c)), mu));
}

void vPrint2(Vector2 a) { printf("(%.2f, %.2f)", a.x, a.y); }

void vPrint3(Vector3 a) { printf("(%.2f, %.2f, %.2f)", a.x, a.y, a.z); }

float hemisphereZ(float x, float y, float radius) {
  return sqrtf(radius * radius - x * x - y * y);
}

float hemisphereSurfacePointX(float x, float y, float radius) {
  return -x / sqrtf(radius * radius - x * x - y * y);
}

float hemisphereSurfacePointY(float x, float y, float radius) {
  return -y / sqrtf(radius * radius - x * x - y * y);
}

void applyLensPixel(Image *dst, Image *src, Vector2 lensCenter, bool showDebug,
                    int x, int y, int width, int height) {
  Color *srcPixels = src->data;
  Color *dstPixels = dst->data;
  int srcCount = src->width * src->height;
  Vector2 pixelCoord = {(float)x, (float)y};
  float distance = Vector2Distance(pixelCoord, lensCenter);
  int dstIndex = y * dst->width + x;

  if (distance <= global_lensRadius) {
    float mu = 1.0f / global_lensIOR;

    float xx = pixelCoord.x - lensCenter.x;
    float yy = pixelCoord.y - lensCenter.y;
    float zz = hemisphereZ(xx, yy, global_lensRadius);

    Vector3 surfacePoint = {xx, yy, zz};
    Vector3 nNormal = Vector3Normalize(surfacePoint);

    Vector3 iLightDirection = {0, 0, -1};
    Vector3 tr = vRefract(iLightDirection, nNormal, mu);
    if (tr.x == 0.0f && tr.y == 0.0f && tr.z == 0.0f) {
      return;
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

void applyLensOn(Image *dst, Image *src, int offsetX, int offsetY,
                 float scale) {
  Vector2 nMousePosition = GetMousePosition();
  Vector2 lensCenter = {(nMousePosition.x - offsetX) / scale,
                        (nMousePosition.y - offsetY) / scale};
  bool showDebug = IsKeyPressed(KEY_D);
  int width = src->width < dst->width ? src->width : dst->width;
  int height = src->height < dst->height ? src->height : dst->height;

  if (global_useParallel) {
#if defined(_OPENMP)
#pragma omp parallel for
#endif /* defined(_OPENMP) */
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        applyLensPixel(dst, src, lensCenter, showDebug, x, y, width, height);
      }
    }
  } else {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        applyLensPixel(dst, src, lensCenter, showDebug, x, y, width, height);
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

  Shader lensShader = LoadShader(0, "resources/lens_shader.glsl");
  int mouseLoc = GetShaderLocation(lensShader, "mouse");
  int radiusLoc = GetShaderLocation(lensShader, "radius");
  int iorLoc = GetShaderLocation(lensShader, "ior");
  int offsetLoc = GetShaderLocation(lensShader, "offset");
  int textureSizeLoc = GetShaderLocation(lensShader, "textureSize");
  int scaleLoc = GetShaderLocation(lensShader, "scale");

  Image sourceImage = {0};
  Texture2D texture = {0};
  if (imagePath) {
    sourceImage = LoadImage(imagePath);
  }

#if defined(PLATFORM_WEB)
  global_useShaderMode = false;
  sourceImage = LoadImage("resources/fruits.png");
  BrowserInitFileDropBridge();
#endif /* defined(PLATFORM_WEB) */

  if (IsImageValid(sourceImage)) {
    ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    texture = LoadTextureFromImage(sourceImage);
  }

  while (!WindowShouldClose()) {
#if defined(PLATFORM_WEB)
    int droppedCount = BrowserDroppedFilesCount();
    if (droppedCount > 0) {
      char droppedPath[2048] = {0};
      Image tempImage = {0};

      BrowserDroppedFilePath(0, droppedPath, sizeof(droppedPath));
      tempImage = LoadImage(droppedPath);
      if (IsImageValid(tempImage)) {
        UnloadImage(sourceImage);
        sourceImage = ImageCopy(tempImage);
        ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        UnloadTexture(texture);
        texture = LoadTextureFromImage(sourceImage);
        global_isLensEnabled = true;

        UnloadImage(tempImage);
        printf("Loaded image: %s\n", droppedPath);
      } else {
        printf("Failed to load image: %s\n", droppedPath);
      }

      BrowserClearDroppedFiles();
    }
#else
    if (IsFileDropped()) {
      FilePathList droppedFiles = LoadDroppedFiles();
      Image tempImage = {0};
      if (droppedFiles.count > 0) {
        tempImage = LoadImage(droppedFiles.paths[0]);
        if (IsImageValid(tempImage)) {
          UnloadImage(sourceImage);
          sourceImage = ImageCopy(tempImage);
          ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

          UnloadTexture(texture);
          texture = LoadTextureFromImage(sourceImage);
          global_isLensEnabled = true;

          UnloadImage(tempImage);
          printf("Loaded image: %s\n", droppedFiles.paths[0]);
        } else {
          printf("Failed to load image: %s\n", droppedFiles.paths[0]);
        }
      }
      UnloadDroppedFiles(droppedFiles);
    }
#endif /* defined(PLATFORM_WEB) */

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      global_isLensEnabled = !global_isLensEnabled;
      global_readyToReset = true;
    }

    if (IsKeyPressed(KEY_SPACE)) {
      global_useShaderMode = !global_useShaderMode;
      global_readyToReset = true;
    }

    if (IsKeyPressed(KEY_P)) {
      global_useParallel = !global_useParallel;
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
      float scaleX = (float)GetScreenWidth() / (float)texture.width;
      float scaleY = (float)GetScreenHeight() / (float)texture.height;
      float scale = scaleX < scaleY ? scaleX : scaleY;
      float textureWidth = (float)texture.width * scale;
      float textureHeight = (float)texture.height * scale;
      Rectangle textureRect = {
          (GetScreenWidth() - textureWidth) / 2.0f,
          (GetScreenHeight() - textureHeight) / 2.0f,
          textureWidth,
          textureHeight,
      };

      if (global_isLensEnabled && global_useShaderMode) {
        global_readyToReset = false;

        float mousePos[2] = {GetMousePosition().x, GetMousePosition().y};
        float lensRadius[1] = {global_lensRadius};
        float lensIOR[1] = {global_lensIOR};
        float offset[2] = {textureRect.x, textureRect.y};
        float imageScale[1] = {scale};
        float textureSize[2] = {(float)texture.width, (float)texture.height};

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
      char *parallelModeText =
          global_useParallel ? "Parallel CPU Mode" : "Sequential CPU Mode";

      DrawRectangle(0, 0, controlsTextWidth * 1.1f + controlsTextWidth * .2f,
                    controlsTextWidth * 1.2f, CLITERAL(Color){0, 0, 0, 150});

      DrawFPS(10, 10);
      DrawText(
          TextFormat("R: %.1f | IOR: %.1f", global_lensRadius, global_lensIOR),
          10, 35, 15, WHITE);
      DrawText(controlsText, 10, 65, 15, WHITE);

      int parallelModeTextY = 185;
      if (global_useShaderMode) {
        parallelModeTextY = parallelModeTextY - 20;
      } else {
        DrawText(parallelModeText, 10, parallelModeTextY, 15,
                 global_useParallel ? GREEN : RED);
      }

      DrawText(shaderModeText, 10, parallelModeTextY + 20, 20,
               global_useShaderMode ? GREEN : RED);

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
  if (IsTextureValid(texture)) {
    UnloadTexture(texture);
  }
  UnloadShader(lensShader);
  CloseWindow();
  return 0;
}
