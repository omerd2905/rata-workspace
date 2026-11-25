#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"

/* Pure C11 icon generator for Rata Workspace (invoked via `make icons`).
 * Generates assets/rata-workspace.png (256x256) and multi-resolution
 * assets/rata-workspace.ico (256, 48, 32, 16 PNG-compressed entries). */

static void put_le16(FILE *f, uint16_t v) {
    fputc((int)(v & 0xFFu), f);
    fputc((int)((v >> 8u) & 0xFFu), f);
}

static void put_le32(FILE *f, uint32_t v) {
    fputc((int)(v & 0xFFu), f);
    fputc((int)((v >> 8u) & 0xFFu), f);
    fputc((int)((v >> 16u) & 0xFFu), f);
    fputc((int)((v >> 24u) & 0xFFu), f);
}

static Image render_icon_256(void) {
    const int size = 256;
    Image img = GenImageColor(size, size, (Color){0, 0, 0, 0});

    Color bg = {38, 28, 22, 255};        /* Coffee Bean */
    Color border = {118, 110, 89, 255};  /* Muted Oak Border */
    Color wave = {232, 221, 203, 255};   /* Pale Oak */
    Color grid = {56, 53, 42, 255};      /* Dark Khaki Grid */
    Color axis = {94, 86, 66, 255};      /* Khaki Zero Axis */
    Color dot = {194, 156, 108, 255};    /* Warm Ochre */

    float rad = 44.0f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = fabsf((float)x - 127.5f) - (120.0f - rad);
            float dy = fabsf((float)y - 127.5f) - (120.0f - rad);
            float ox = (dx > 0.0f) ? dx : 0.0f;
            float oy = (dy > 0.0f) ? dy : 0.0f;
            float inside = (dx > dy) ? dx : dy;
            float dist = sqrtf(ox * ox + oy * oy) + ((dx > 0.0f || dy > 0.0f) ? 0.0f : inside);
            if (dist <= rad) {
                Color c = (dist >= rad - 6.0f) ? border : bg;
                ImageDrawPixel(&img, x, y, c);
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        int gy = 76 + i * 52;
        int gx = 76 + i * 52;
        ImageDrawRectangle(&img, 32, gy - 1, 192, 3, (i == 1) ? axis : grid);
        ImageDrawRectangle(&img, gx - 1, 36, 3, 184, grid);
    }

    int prev_x = 34;
    int prev_y = 128;
    for (int px = 34; px <= 222; ++px) {
        double u = (double)(px - 34) / 188.0;
        double val = exp(-1.15 * u) * sin(2.0 * 3.14159265358979323846 * 2.0 * u);
        int py = 128 - (int)round(val * 74.0);
        for (int ox = -5; ox <= 5; ++ox) {
            for (int oy = -5; oy <= 5; ++oy) {
                if (ox * ox + oy * oy <= 25) {
                    ImageDrawLine(&img, prev_x + ox, prev_y + oy, px + ox, py + oy, wave);
                }
            }
        }
        prev_x = px;
        prev_y = py;
    }

    const double marks[4] = {0.125, 0.375, 0.625, 0.875};
    for (int i = 0; i < 4; ++i) {
        double u = marks[i];
        int px = 34 + (int)round(u * 188.0);
        double val = exp(-1.15 * u) * sin(2.0 * 3.14159265358979323846 * 2.0 * u);
        int py = 128 - (int)round(val * 74.0);
        ImageDrawCircle(&img, px, py, 9, dot);
    }

    return img;
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    Image img256 = render_icon_256();
    if (!ExportImage(img256, "assets/rata-workspace.png")) {
        fprintf(stderr, "gen_icon: failed to write assets/rata-workspace.png\n");
        UnloadImage(img256);
        return 1;
    }

    const int sizes[4] = {256, 48, 32, 16};
    unsigned char *pngs[4] = {NULL, NULL, NULL, NULL};
    int png_lens[4] = {0, 0, 0, 0};

    for (int i = 0; i < 4; ++i) {
        Image copy = ImageCopy(img256);
        if (sizes[i] != 256) {
            ImageResize(&copy, sizes[i], sizes[i]);
        }
        pngs[i] = ExportImageToMemory(copy, ".png", &png_lens[i]);
        UnloadImage(copy);
        if (pngs[i] == NULL || png_lens[i] <= 0) {
            fprintf(stderr, "gen_icon: failed to encode PNG size %d\n", sizes[i]);
            UnloadImage(img256);
            return 1;
        }
    }

    FILE *fp = fopen("assets/rata-workspace.ico", "wb");
    if (fp == NULL) {
        fprintf(stderr, "gen_icon: failed to open assets/rata-workspace.ico\n");
        for (int i = 0; i < 4; ++i) {
            MemFree(pngs[i]);
        }
        UnloadImage(img256);
        return 1;
    }

    put_le16(fp, 0u);
    put_le16(fp, 1u);
    put_le16(fp, 4u);

    uint32_t offset = 6u + 4u * 16u;
    for (int i = 0; i < 4; ++i) {
        uint8_t dim = (sizes[i] >= 256) ? 0u : (uint8_t)sizes[i];
        fputc((int)dim, fp);
        fputc((int)dim, fp);
        fputc(0, fp);
        fputc(0, fp);
        put_le16(fp, 1u);
        put_le16(fp, 32u);
        put_le32(fp, (uint32_t)png_lens[i]);
        put_le32(fp, offset);
        offset += (uint32_t)png_lens[i];
    }
    for (int i = 0; i < 4; ++i) {
        fwrite(pngs[i], 1u, (size_t)png_lens[i], fp);
        MemFree(pngs[i]);
    }
    fclose(fp);
    UnloadImage(img256);
    printf("Generated assets/rata-workspace.png and assets/rata-workspace.ico\n");
    return 0;
}
