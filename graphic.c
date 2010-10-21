#include <stdint.h>
#include <stdio.h>
#include <png.h>

#include "graphic.h"

static inline uint8_t getbit(uint8_t src, int index) {
    return (src & (1 << index)) >> index;
}
static inline uint8_t putbit(uint8_t bit, int index) {
    return bit << index;
}
static inline uint8_t swapbits(uint8_t value) {
    return (value << 4) | (value >> 4);
}

// 转换SNES格式的8x8像素Tile为BMP格式
void snes_tile_to_bmp_tile(const void *snesTile, void *bmpTile) {
    const uint8_t *src = snesTile;
    uint8_t *dst = bmpTile;
    for (int row = 0; row < 8; ++row) {
        uint8_t bitplane0 = src[0x00];
        uint8_t bitplane1 = src[0x01];
        uint8_t bitplane2 = src[0x10];
        uint8_t bitplane3 = src[0x11];
        for (int bitindex = 7; bitindex > 0; bitindex -= 2) {
            *dst =
                putbit(getbit(bitplane3, bitindex    ), 7) |
                putbit(getbit(bitplane2, bitindex    ), 6) |
                putbit(getbit(bitplane1, bitindex    ), 5) |
                putbit(getbit(bitplane0, bitindex    ), 4) |
                putbit(getbit(bitplane3, bitindex - 1), 3) |
                putbit(getbit(bitplane2, bitindex - 1), 2) |
                putbit(getbit(bitplane1, bitindex - 1), 1) |
                putbit(getbit(bitplane0, bitindex - 1), 0);
            dst += 1;
        }
        src += 2;
    }
}

// 转换SNES格式的Tile数组为BMP格式的像素数组
void snes_tiles_to_bmp_pixels(const void *snesTiles, void *bmpPixels, int width, int height) {
    const uint8_t (*src)[0x20] = snesTiles;
    int wtile = width >> 3;
    int htile = height >> 3;
    uint8_t bmpTile[0x20];
    for (int hi = 0; hi < htile; ++hi) {
        for (int wi = 0; wi < wtile; ++wi) {
            snes_tile_to_bmp_tile(src + hi * wtile + wi, bmpTile);
            bmp_pixels_copy_rect(bmpTile, 8, 8, 0, 0,
                bmpPixels, width, height, wi << 3, hi << 3, 8, 8, false);

        }
    }
}

// 复制图像区域
void bmp_pixels_copy_rect(
    const void *source, int sourceWidth, int sourceHeight, int sourceX, int sourceY,
          void *target, int targetWidth, int targetHeight, int targetX, int targetY,
          int copyWidth, int copyHeight, bool flipHorizontal) {
    const uint8_t *src = source;
    uint8_t *dst = target;

    // 像素数 -> 字节数
    sourceWidth >>= 1; sourceX >>= 1;
    targetWidth >>= 1; targetX >>= 1;
    copyWidth >>= 1;

    // 复制
    for (int y = 0; y < copyHeight; ++y) {
        for (int x = 0; x < copyWidth; ++x) {
            dst[(targetY + y) * targetWidth + (targetX + x)] = flipHorizontal ?
                swapbits(src[(sourceY + y) * sourceWidth + (sourceX + copyWidth - x - 1)]) :
                src[(sourceY + y) * sourceWidth + (sourceX + x)];
        }
    }
}

// 转换SNES格式调色板(BGR555)为BMP调色板(ARGB32)
void snes_palette_to_bmp_palette(const void *snesPalette, void *bmpPalette) {
    // 16bit: 0 bbbbb ggggg rrrrr
    const struct {
        uint16_t r :5;
        uint16_t g :5;
        uint16_t b :5;
        uint16_t   :1;
    } *src = snesPalette;

    // 32bit: 00000000 rrrrr000 ggggg000 bbbbb000
    struct {
        uint32_t :3, b :5;
        uint32_t :3, g :5;
        uint32_t :3, r :5;
        uint32_t :8;
    } *dst = bmpPalette;

    for (int i = 0; i < 0x10; ++i) {
        *(uint32_t *)(dst + i) = 0;
        dst[i].r = src[i].r;
        dst[i].g = src[i].g;
        dst[i].b = src[i].b;
    }
}


void bmp_write_file(char *path, void *palette, void *pixels, int width, int height) {
    static uint8_t bmpHeader[0x36] = {
        0x42, 0x4D,             // Bitmap Sign
        0xFF, 0xFF, 0xFF, 0xFF, // File Size
        0x00, 0x00, 0x00, 0x00, // Reserved
        0x76, 0x00, 0x00, 0x00, // Bitmap Data Offset
        0x28, 0x00, 0x00, 0x00, // Bitmap Header Size
        0xFF, 0xFF, 0xFF, 0xFF, // Width
        0xFF, 0xFF, 0xFF, 0xFF, // Height
        0x01, 0x00,             // Planes
        0x04, 0x00,             // Bits Per Pixel
        0x00, 0x00, 0x00, 0x00, // Compression
        0xFF, 0xFF, 0xFF, 0xFF, // Bitmap Data Size
        0x00, 0x00, 0x00, 0x00, // HResolution
        0x00, 0x00, 0x00, 0x00, // VResolution
        0x00, 0x00, 0x00, 0x00, // Colors
        0x00, 0x00, 0x00, 0x00  // Important Colors
    };
    int paletteSize = 0x40;
    int pixelsSize = (width >> 1) * height;
    *(int *)(bmpHeader + 0x02) = (sizeof bmpHeader) + paletteSize + pixelsSize;
    *(int *)(bmpHeader + 0x12) = width;
    *(int *)(bmpHeader + 0x16) = -height;
    *(int *)(bmpHeader + 0x22) = pixelsSize;

    FILE *file = fopen(path, "wb");
    fwrite(bmpHeader, sizeof bmpHeader, 1, file);
    fwrite(palette, paletteSize, 1, file);
    fwrite(pixels, pixelsSize, 1, file);
    fclose(file);
}

void png_write_file(char *path, void *palette, void *pixels, int width, int height) {
    FILE *file = fopen(path, "wb");

    png_struct *pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info *pngInfo = png_create_info_struct(pngStruct);

    png_init_io(pngStruct, file);

    png_set_IHDR(pngStruct, pngInfo, width, height, 4, PNG_COLOR_TYPE_PALETTE,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    struct { uint8_t b, g, r, _; } *bmpPalette = palette;
    png_color *pngPalette = png_malloc(pngStruct, 0x10 * sizeof(png_color));
    for (int i = 0; i < 0x10; ++i) {
        pngPalette[i].red = bmpPalette[i].r;
        pngPalette[i].green = bmpPalette[i].g;
        pngPalette[i].blue = bmpPalette[i].b;
    }
    png_set_PLTE(pngStruct, pngInfo, pngPalette, 0x10);
    png_set_tRNS(pngStruct, pngInfo, "\0", 1, NULL);

    png_write_info(pngStruct, pngInfo);

    png_byte **pngMap = png_malloc(pngStruct, height * sizeof(png_byte *));;
    for (int i = 0; i < height; ++i) {
        pngMap[i] = (png_byte *)pixels + (i * width >> 1);
    }
    png_write_image(pngStruct, pngMap);

    png_write_end(pngStruct, pngInfo);
    png_free(pngStruct, pngPalette);
    png_free(pngStruct, pngMap);
    png_destroy_write_struct(&pngStruct, &pngInfo);
    fclose(file);
}
