#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "decompress.h"
#include "graphic.h"

struct tile {
    struct tile *next;
    uint32_t fileAddr;
    uint32_t length;
    uint8_t dataCompressed[128 * 32 / 2];
    uint8_t dataSnes[128 * 32 / 2];
    uint8_t dataPixels[128 * 32 / 2];
    uint8_t dataPixelsDisplay[48 * 64 / 2];
    bool hasSpeakArea;
    uint8_t dataPixelsSpeak1[48 * 64 / 2];
    uint8_t dataPixelsSpeak2[48 * 64 / 2];
};

struct palette {
    struct palette *next;
    uint32_t fileAddr;
    uint8_t dataSnes[0x20];
    uint8_t dataBmp[0x40];
};

struct portrait {
    uint32_t tileSnesAddr;
    uint32_t paletteSnesAddr;
    struct tile *tile;
    struct palette *palette;
};

static inline uint32_t snes_address_to_file_address(uint32_t snesAddr) {
    return snesAddr & 0x3FFFFF;
}

static char* filepath_sprintf(char *template, int index) {
    static char filepath[260];
    sprintf(filepath, template, index);
    return filepath;
}

int main4(int argc, char **argv) {
    FILE *rom = fopen(".\\FE4.sfc", "rb");
    fseek(rom, 0, SEEK_END);
    if (ftell(rom) != 0x400000) {
        printf("Must use no header ROM\n");
        return -1;
    }

    const int portraitCount = 248;
    struct portrait portraits[portraitCount];

    // 读取头像Tile表(每项为3字节指针)
    fseek(rom, 0x0AB4F9, SEEK_SET);  // Tile表地址
    for (int i = 0; i < portraitCount; ++i) {
        fread(&portraits[i].tileSnesAddr, 3, 1, rom);
    }

    // 初始化Tile并和头像表关联
    struct tile *tiles = malloc(sizeof(struct tile));
    tiles->next = malloc(sizeof(struct tile));
    tiles->next->next = NULL;
    tiles->next->fileAddr = 0x105639;  // 头像Tile数据结束
    for (int i = 0; i < portraitCount; ++i) {
        uint32_t addr = snes_address_to_file_address(portraits[i].tileSnesAddr);
        struct tile *tile = tiles;
        while (tile->next->fileAddr < addr) {
            tile = tile->next;
        }
        if (tile->next->fileAddr != addr) {
            struct tile *newNode = malloc(sizeof(struct tile));
            newNode->next = tile->next;
            newNode->fileAddr = addr;
            tile->next = newNode;
        }
        portraits[i].tile = tile->next;
    }

    // 读取并处理Tile内容
    struct tile *tile = tiles->next;
    fseek(rom, tile->fileAddr, SEEK_SET);
    while (tile->next != NULL) {
        // 读取并解压缩
        tile->length = tile->next->fileAddr - tile->fileAddr;
        if (tile->length <= 0x7FFF) {
            fread(tile->dataCompressed, tile->length, 1, rom);
        } else {
            // 读取时需要跳过0x??8000-0x??FFFF地址范围
            tile->length &= 0x7FFF;
            int part2Length = tile->next->fileAddr & 0x7FFF;
            int part1Length = tile->length - part2Length;
            fread(tile->dataCompressed, part1Length, 1, rom);
            fseek(rom, 0x8000, SEEK_CUR);
            fread(tile->dataCompressed + part1Length, part2Length, 1, rom);
        }
        if(decompress(tile->dataCompressed, tile->dataSnes, 0x800) != tile->length) {
            printf("Tile length not equal: File Address %06X\n", tile->fileAddr);
        }

        // 转换为像素数组
        snes_tiles_to_bmp_pixels(tile->dataSnes, tile->dataPixels, 128, 32);

        // 拼接为游戏里实际看到的样子
        bmp_pixels_copy_rect(tile->dataPixels, 128, 32,  0, 0,
            tile->dataPixelsDisplay, 48, 64, 0,  0, 48, 32, false);
        bmp_pixels_copy_rect(tile->dataPixels, 128, 32, 48, 0,
            tile->dataPixelsDisplay, 48, 64, 0, 32, 48, 32, false);

        // 拼接带说话动作的版本
        tile->hasSpeakArea = tile->dataPixels[60] != 0x00;
        if (tile->hasSpeakArea) {
            memcpy(tile->dataPixelsSpeak1, tile->dataPixelsDisplay, 48 * 64 / 2);
            bmp_pixels_copy_rect(tile->dataPixels, 128, 32, 96, 0,
                tile->dataPixelsSpeak1, 48, 64, 16, 32, 32, 16, false);
            memcpy(tile->dataPixelsSpeak2, tile->dataPixelsDisplay, 48 * 64 / 2);
            bmp_pixels_copy_rect(tile->dataPixels, 128, 32, 96, 16,
                tile->dataPixelsSpeak2, 48, 64, 16, 32, 32, 16, false);
        }

        tile = tile->next;
    }

    // 读取头像调色板表(每项为3字节指针)
    fseek(rom, 0x0AB7E1, SEEK_SET);  // 调色板表地址
    for (int i = 0; i < portraitCount; ++i) {
        fread(&portraits[i].paletteSnesAddr, 3, 1, rom);
    }

    // 初始化调色板并和头像表关联
    struct palette *palettes = malloc(sizeof(struct palette));
    palettes->next = malloc(sizeof(struct palette));
    palettes->next->next = NULL;
    palettes->next->fileAddr = 0xFFFFFF;
    for (int i = 0; i < portraitCount; ++i) {
        uint32_t addr = snes_address_to_file_address(portraits[i].paletteSnesAddr);
        struct palette *palette = palettes;
        while (palette->next->fileAddr < addr) {
            palette = palette->next;
        }
        if (palette->next->fileAddr != addr) {
            struct palette *newNode = malloc(sizeof(struct palette));
            newNode->next = palette->next;
            newNode->fileAddr = addr;
            palette->next = newNode;
        }
        portraits[i].palette = palette->next;
    }

    // 读取并处理调色板内容
    struct palette *palette = palettes->next;
    while (palette->next != NULL) {
        fseek(rom, palette->fileAddr, SEEK_SET);
        fread(palette->dataSnes, 0x20, 1, rom);
        snes_palette_to_bmp_palette(palette->dataSnes, palette->dataBmp);
        palette = palette->next;
    }

    mkdir(".\\FE4");
    mkdir(".\\FE4\\bmp");
    mkdir(".\\FE4\\png");
    mkdir(".\\FE4\\png_speak");
    for (int i = 0; i < portraitCount; ++i) {
        // 输出BMP(128x32)
        bmp_write_file(filepath_sprintf(".\\FE4\\bmp\\%03d.bmp", i),
            portraits[i].palette->dataBmp, portraits[i].tile->dataPixels, 128, 32);
        // 输出PNG(48x64)
        png_write_file(filepath_sprintf(".\\FE4\\png\\%03d.png", i),
            portraits[i].palette->dataBmp, portraits[i].tile->dataPixelsDisplay, 48, 64);
        // 输出PNG(说话)
        if (portraits[i].tile->hasSpeakArea) {
            png_write_file(filepath_sprintf(".\\FE4\\png_speak\\%03d_1.png", i),
                portraits[i].palette->dataBmp, portraits[i].tile->dataPixelsSpeak1, 48, 64);
            png_write_file(filepath_sprintf(".\\FE4\\png_speak\\%03d_2.png", i),
                portraits[i].palette->dataBmp, portraits[i].tile->dataPixelsSpeak2, 48, 64);
        }
    }

    fclose(rom);
    // free(tiles);
    return 0;
}
