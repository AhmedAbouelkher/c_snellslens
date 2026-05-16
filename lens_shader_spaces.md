```text
Screen / window space
(what the mouse and pixels live in)
+------------------------------------------------+
|                                                |
|            mouse = (x, y)                     |
|                |                               |
|                v                               |
|          +-------------+                       |
|          |  image area  |   offset = (ox, oy)  |
|          +-------------+                       |
|                                                |
+------------------------------------------------+

Image space
(pixels inside the image)
pixelCoord = fragTexCoord * textureSize + offset

mouse and pixelCoord are compared here:
distance(pixelCoord, mouse) < radius

Then convert back to UV space for sampling:
uv = (sampleCoord - offset) / textureSize


UV space
(what texture() expects)
(0,0) ----------------------------------> (1,0)
  |                                         |
  |                                         |
  |                                         |
  v                                         v
(0,1) ----------------------------------> (1,1)
```

Simple version:

- **mouse / radius / offset** = pixel space
- **lens math** = pixel space
- **texture() sampling** = UV space

So the flow is:

```text
mouse position in pixels
        ->
lens math in pixels
        ->
convert to UV
        ->
sample texture
```
