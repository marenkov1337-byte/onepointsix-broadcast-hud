#pragma once

// Loads an uncompressed 24 or 32-bit Windows BMP (the format CS 1.6's native
// "overviews/<map>.bmp" files use) and uploads it as an OpenGL RGBA texture,
// mirroring load_png_texture's interface/behavior.
auto load_bmp_texture(const char *filename, int *width, int *height) -> unsigned int;
