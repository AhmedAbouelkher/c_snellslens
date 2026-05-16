# Snell's Lens

A small Raylib demo that bends an image through a simple glass lens using Snell's law.

## What it does

- `main.c` loads the image, handles input, and switches between CPU and shader rendering.
- `vector_math.h` provides the vector helpers and refraction math.
- `lens_shader.glsl` applies the same lens effect on the GPU.

## Controls

- Drag and drop an image to load it.
- Right click to toggle the lens.
- `Space` to switch CPU / shader mode.
- `W` / `S` to change lens radius.
- `I` / `O` to change index of refraction.

## How it works

The lens is modeled as a hemisphere. For every pixel inside the lens radius, the code computes the surface normal, refracts a ray using Snell's law in vector form, and samples the source image at the refracted position.

## References

- [Snell's law in vector form](https://physics.stackexchange.com/questions/435512/snells-law-in-vector-form)
- [OpenGL `refract`](https://registry.khronos.org/OpenGL-Refpages/gl4/html/refract.xhtml)
- [Video reference](https://www.youtube.com/watch?v=0OvhpQVTS2Q)
- [Desmos sketch](https://www.desmos.com/3d/eebbaskabr)
