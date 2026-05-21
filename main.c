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
#endif

float global_lensRadius = 100.0f;
float global_lensIOR = 1.52f;
bool global_useShaderMode = true;
bool global_isLensEnabled = false;
bool global_useParallel = true;

typedef struct {
  Rectangle rect;
  const char *label;
  float *value;
  float delta;
  float minValue;
  float maxValue;
} StepButton;

typedef struct {
  StepButton radiusDecreaseButton;
  StepButton radiusIncreaseButton;
  StepButton iorDecreaseButton;
  StepButton iorIncreaseButton;
} WebControlPanel;

static void applyStepButton(StepButton button) {
  *button.value += button.delta;
  if (*button.value > button.maxValue) {
    *button.value = button.maxValue;
  }
  if (*button.value < button.minValue) {
    *button.value = button.minValue;
  }
}

static void drawStepButton(StepButton button) {
  bool hovered = CheckCollisionPointRec(GetMousePosition(), button.rect);
  DrawRectangleRec(button.rect, hovered ? DARKGRAY : GRAY);
  DrawRectangleLinesEx(button.rect, 2.0f, WHITE);
  int textWidth = MeasureText(button.label, 24);
  DrawText(button.label,
           (int)(button.rect.x + (button.rect.width - textWidth) / 2.0f),
           (int)(button.rect.y + 18.0f), 24, WHITE);
}

static void handleStepButton(StepButton button) {
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      CheckCollisionPointRec(GetMousePosition(), button.rect)) {
    applyStepButton(button);
  }
}

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

void UpdateDrawFrame(void);

Shader lensShader = {0};
Texture2D texture = {0};
Image sourceImage = {0};
int mouseLoc = 0, radiusLoc = 0, iorLoc = 0, offsetLoc = 0, textureSizeLoc = 0,
    scaleLoc = 0;

int main(int argc, char **argv) {
  char *imagePath = NULL;
  if (argc > 1) {
    imagePath = argv[1];
  }

  InitWindow(1200, 900, "Snell's Lens");

#if defined(PLATFORM_WEB)
  lensShader = LoadShader(0, "resources/glsl100/lens_shader.glsl");
#else
  lensShader = LoadShader(0, "resources/lens_shader.glsl");
#endif /* defined(PLATFORM_WEB) */
  mouseLoc = GetShaderLocation(lensShader, "mouse");
  radiusLoc = GetShaderLocation(lensShader, "radius");
  iorLoc = GetShaderLocation(lensShader, "ior");
  offsetLoc = GetShaderLocation(lensShader, "offset");
  textureSizeLoc = GetShaderLocation(lensShader, "textureSize");
  scaleLoc = GetShaderLocation(lensShader, "scale");

  if (imagePath) {
    sourceImage = LoadImage(imagePath);
  }

#if defined(PLATFORM_WEB)
  sourceImage = LoadImage(
      "resources/"
      "pedestrian-road-street-city-urban-new-york-1002217-pxhere.com.png");
  // BrowserInitFileDropBridge();
  global_isLensEnabled = true;
#endif /* defined(PLATFORM_WEB) */

  if (IsImageValid(sourceImage)) {
    ImageFormat(&sourceImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    texture = LoadTextureFromImage(sourceImage);
  }

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else

  while (!WindowShouldClose()) {
    UpdateDrawFrame();
  }
#endif

  UnloadImage(sourceImage);
  if (IsTextureValid(texture)) {
    UnloadTexture(texture);
  }
  UnloadShader(lensShader);
  CloseWindow();
  return 0;
}

void UpdateDrawFrame(void) {
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

  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    global_isLensEnabled = !global_isLensEnabled;
    UpdateTexture(texture, sourceImage.data);
  }

  if (IsKeyPressed(KEY_SPACE)) {
    global_useShaderMode = !global_useShaderMode;
    UpdateTexture(texture, sourceImage.data);
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

      applyLensOn(&frame, &sourceImage, (int)textureRect.x, (int)textureRect.y,
                  scale);
      UpdateTexture(texture, frame.data);
      UnloadImage(frame);
    }

    if (!(global_isLensEnabled && global_useShaderMode)) {
      DrawTexturePro(texture,
                     CLITERAL(Rectangle){0.0f, 0.0f, (float)texture.width,
                                         (float)texture.height},
                     textureRect, CLITERAL(Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }

#if defined(PLATFORM_WEB)
    WebControlPanel webControlPanel = {
        .radiusDecreaseButton =
            {
                .rect = {(float)GetScreenWidth() / 2.0f - 270.0f,
                         (float)GetScreenHeight() - 70.0f, 120.0f, 50.0f},
                .label = "R-",
                .value = &global_lensRadius,
                .delta = -10.0f,
                .minValue = 30.0f,
                .maxValue = 300.0f,
            },
        .radiusIncreaseButton =
            {
                .rect = {(float)GetScreenWidth() / 2.0f - 130.0f,
                         (float)GetScreenHeight() - 70.0f, 120.0f, 50.0f},
                .label = "R+",
                .value = &global_lensRadius,
                .delta = 10.0f,
                .minValue = 30.0f,
                .maxValue = 300.0f,
            },
        .iorDecreaseButton =
            {
                .rect = {(float)GetScreenWidth() / 2.0f + 10.0f,
                         (float)GetScreenHeight() - 70.0f, 120.0f, 50.0f},
                .label = "IOR-",
                .value = &global_lensIOR,
                .delta = -0.1f,
                .minValue = 1.25f,
                .maxValue = 7.0f,
            },
        .iorIncreaseButton =
            {
                .rect = {(float)GetScreenWidth() / 2.0f + 150.0f,
                         (float)GetScreenHeight() - 70.0f, 120.0f, 50.0f},
                .label = "IOR+",
                .value = &global_lensIOR,
                .delta = 0.1f,
                .minValue = 1.25f,
                .maxValue = 7.0f,
            },
    };

    handleStepButton(webControlPanel.radiusDecreaseButton);
    handleStepButton(webControlPanel.radiusIncreaseButton);
    handleStepButton(webControlPanel.iorDecreaseButton);
    handleStepButton(webControlPanel.iorIncreaseButton);
#endif

    char *controlsText = "[Right Click] Toggle Lens\n[W] Increase "
                         "Radius\n[S] Decrease Radius\n[I] Increase IOR"
                         "\n[O] Decrease IOR\n[Space] Toggle Shader/CPU";
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
        10, 35, 20, WHITE);
    DrawText(controlsText, 10, 65, 20, WHITE);

    int parallelModeTextY = 185;
    if (global_useShaderMode) {
      parallelModeTextY = parallelModeTextY - 20;
    } else {
      DrawText(parallelModeText, 10, parallelModeTextY + 20, 15,
               global_useParallel ? GREEN : RED);
    }

    DrawText(shaderModeText, 10, parallelModeTextY + 40, 20,
             global_useShaderMode ? GREEN : RED);

#if defined(PLATFORM_WEB)
    drawStepButton(webControlPanel.radiusDecreaseButton);
    drawStepButton(webControlPanel.radiusIncreaseButton);
    drawStepButton(webControlPanel.iorDecreaseButton);
    drawStepButton(webControlPanel.iorIncreaseButton);
#endif

  } else {
    const char *text = "Drag and drop an image to see the Snell's Lens effect";
    int textHeight = 30;
    int textWidth = MeasureText(text, textHeight);
    DrawText(text, (GetScreenWidth() - textWidth) / 2,
             (GetScreenHeight() - textHeight) / 2, textHeight, WHITE);
  }

  EndDrawing();
}