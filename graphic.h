#ifndef __graphic_h__
#define __graphic_h__

#include <stdbool.h>

void snes_tile_to_bmp_tile(const void *snesTile, void *bmpTile);
void snes_tiles_to_bmp_pixels(const void *snesTiles, void *bmpPixels, int width, int height);

void bmp_pixels_copy_rect(
    const void *source, int sourceWidth, int sourceHeight, int sourceX, int sourceY,
          void *target, int targetWidth, int targetHeight, int targetX, int targetY,
          int copyWidth, int copyHeight, bool flipHorizontal);

void snes_palette_to_bmp_palette(const void *snesPalette, void *bmpPalette);

void bmp_write_file(char *path, void *palette, void *pixels, int width, int height);
void png_write_file(char *path, void *palette, void *pixels, int width, int height);

#endif // __graphic_h__
