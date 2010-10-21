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
    uint8_t dataSnes[0x20];
    uint8_t dataBmp[0x40];
};

struct portrait {
    uint32_t tileSnesAddr :24;
    uint32_t paletteIndex :8;
    struct tile *tile;
    struct palette *palette;
};

static inline uint32_t snes_address_to_file_address(uint32_t snesAddr) {
    return ((snesAddr & 0x7F0000) >> 1) + (snesAddr & 0x7FFF);
}

static char* filepath_sprintf(char *template, int index) {
    static char filepath[260];
    sprintf(filepath, template, index);
    return filepath;
}

int main5(int argc, char **argv) {
    FILE *rom = fopen(".\\FE5.sfc", "rb");
    fseek(rom, 0, SEEK_END);
    if (ftell(rom) != 0x400000) {
        printf("Must use no header ROM\n");
        return -1;
    }

    const int portraitCount = 250;
    struct portrait portraits[portraitCount];

    // 读取头像表(每项4字节，3字节Tile指针+1字节调色板序号)
    fseek(rom, 0x06512A, SEEK_SET);  // 头像表地址
    for (int i = 0; i < portraitCount - 1; ++i) {
        fread(&portraits[i], 4, 1, rom);
    }
    *((uint32_t *)&portraits[portraitCount - 1]) = 0x23EC9117;  // ROM里头像表数据缺了一条

    // 初始化Tile并和头像表关联
    struct tile *tiles = malloc(sizeof(struct tile));
    tiles->next = malloc(sizeof(struct tile));
    tiles->next->next = NULL;
    tiles->next->fileAddr = 0x37F388;  // 头像Tile数据结束
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
        fread(tile->dataCompressed, tile->length, 1, rom);
        if(decompress(tile->dataCompressed, tile->dataSnes, 0x800) != tile->length) {
            printf("Tile length not equal: File Address %06X\n", tile->fileAddr);
        }

        // 转换为像素数组
        snes_tiles_to_bmp_pixels(tile->dataSnes, tile->dataPixels, 128, 32);

        // 拼接为游戏里实际看到的样子
        bmp_pixels_copy_rect(tile->dataPixels, 128, 32,  0, 0,
            tile->dataPixelsDisplay, 48, 64, 0,  0, 48, 32, true);
        bmp_pixels_copy_rect(tile->dataPixels, 128, 32, 48, 0,
            tile->dataPixelsDisplay, 48, 64, 0, 32, 48, 32, true);

        // 拼接带说话动作的版本
        tile->hasSpeakArea = tile->dataPixels[60] != 0x00;
        if (tile->hasSpeakArea) {
            memcpy(tile->dataPixelsSpeak1, tile->dataPixelsDisplay, 48 * 64 / 2);
            bmp_pixels_copy_rect(tile->dataPixels, 128, 32, 96, 0,
                tile->dataPixelsSpeak1, 48, 64, 16, 32, 32, 16, true);
            memcpy(tile->dataPixelsSpeak2, tile->dataPixelsDisplay, 48 * 64 / 2);
            bmp_pixels_copy_rect(tile->dataPixels, 128, 32, 96, 16,
                tile->dataPixelsSpeak2, 48, 64, 16, 32, 32, 16, true);
        }

        tile = tile->next;
    }

    // 读取并处理调色板内容
    struct palette palettes[0xFF];
    fseek(rom, 0x354000, SEEK_SET);  // 调色板地址
    for (int i = 0; i < 0xFF; ++i) {
        fread(palettes[i].dataSnes, 0x20, 1, rom);
        snes_palette_to_bmp_palette(palettes[i].dataSnes, palettes[i].dataBmp);
    }

    // 关联头像表和调色板内容
    for (int i = 0; i < portraitCount; ++i) {
        portraits[i].palette = palettes + portraits[i].paletteIndex;
    }

    mkdir(".\\FE5");
    mkdir(".\\FE5\\bmp");
    mkdir(".\\FE5\\png");
    mkdir(".\\FE5\\png_speak");
    for (int i = 0; i < portraitCount; ++i) {
        // 输出BMP(128x32)
        bmp_write_file(filepath_sprintf(".\\FE5\\bmp\\%03d.bmp", i),
            portraits[i].palette->dataBmp, portraits[i].tile->dataPixels, 128, 32);
        // 输出PNG(48x64)
        png_write_file(filepath_sprintf(".\\FE5\\png\\%03d.png", i),
            portraits[i].palette->dataBmp, portraits[i].tile->dataPixelsDisplay, 48, 64);
        // 输出PNG(说话)
        if (portraits[i].tile->hasSpeakArea) {
            png_write_file(filepath_sprintf(".\\FE5\\png_speak\\%03d_1.png", i),
                portraits[i].palette->dataBmp, portraits[i].tile->dataPixelsSpeak1, 48, 64);
            png_write_file(filepath_sprintf(".\\FE5\\png_speak\\%03d_2.png", i),
                portraits[i].palette->dataBmp, portraits[i].tile->dataPixelsSpeak2, 48, 64);
        }
    }

    fclose(rom);
    // free(tiles);
    return 0;
}
