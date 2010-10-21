#include <stdint.h>
#include <stdio.h>

#include "decompress.h"

int decompress(const void *compressedData, void *uncompressedData, int maxLength) {
    const uint8_t *src = compressedData;
    uint8_t *dst = uncompressedData;

    while (1) {
        if ((dst - (uint8_t *)uncompressedData) > maxLength) return -1;

        // 0x00 - 0x3F 直接输出
        if (src[0] <= 0x3F) {
            int n = src[0] + 1;
            src += 1;
            while (n-- > 0) {
                *dst++ = *src++;
            }
        } else

        // 0x40 - 0x4F 未使用
        if (src[0] <= 0x4F) {
            printf("unsupported: %02X\n", src[0]);
            return -1;
        } else

        // 0x50 - 0x5F 每字节重复两次
        if (src[0] <= 0x5F) {
            int n = (src[0] & 0b00001111) + 1;
            src += 1;
            while (n-- > 0) {
                *dst++ = *src;
                *dst++ = *src++;
            }
        } else

        // 0x60 - 0x6F 每字节前加入特定字节
        if (src[0] <= 0x6F) {
            int n = (src[0] & 0b00001111) + 2;
            uint8_t x = src[1];
            src += 2;
            while (n-- > 0) {
                *dst++ = x;
                *dst++ = *src++;
            }
        } else

        // 0x70 - 0x7F 每字节后加入特定字节
        if (src[0] <= 0x7F) {
            int n = (src[0] & 0b00001111) + 2;
            uint8_t x = src[1];
            src += 2;
            while (n-- > 0) {
                *dst++ = *src++;
                *dst++ = x;
            }
        } else

        // 0x80 - 0xBF 重复之前的字节序列
        if (src[0] <= 0xBF) {
            int n = ((src[0] & 0b00111100) >> 2) + 2;
            uint8_t *srcx = dst - (((src[0] & 0b00000011) << 8) | src[1]);
            src += 2;
            while (n-- > 0) {
                *dst++ = *srcx++;
            }
        } else

        // 0xC0 - 0xDF 重复之前的字节序列
        if (src[0] <= 0xDF) {
            int n = (((src[0] & 0b00011111) << 1) | ((src[1] & 0b10000000) >> 7)) + 2;
            uint8_t *srcx = dst - (((src[1] & 0b01111111) << 8) | src[2]);
            src += 3;
            while (n-- > 0) {
                *dst++ = *srcx++;
            }
        } else

        // 0xE0 - 0xEF 重复特定字节
        if (src[0] <= 0xEF) {
            int n = (((src[0] & 0b00001111) << 8) | src[1]) + 3;
            src += 2;
            while (n-- > 0) {
                *dst++ = *src;
            }
            src += 1;
        } else

        // 0xF0 - 0xF7 重复特定字节
        if (src[0] <= 0xF7) {
            int n = (src[0] & 0b00001111) + 3;
            src += 1;
            while (n-- > 0) {
                *dst++ = *src;
            }
            src += 1;
        } else

        // 0xF8 - 0xFD 未使用
        if (src[0] <= 0xFD) {
            printf("unsupported: %02X\n", src[0]);
            return -1;
        } else

        // 0xFE - 0xFF 解压结束
        /* if (src[0] <= 0xFF) */{
            return src - (uint8_t *)compressedData + 1;
        }
    }
}
