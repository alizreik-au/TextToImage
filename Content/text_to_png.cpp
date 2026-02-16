// text_to_png.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Helper: clamp
static inline unsigned char clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Usage: %s font.ttf \"Text to render\" size output.png\n", argv[0]);
        return 1;
    }

    const char* fontPath = argv[1];
    const char* text = argv[2];
    int size = atoi(argv[3]); // square size in pixels
    const char* outPath = argv[4];

    // Load font file into memory
    FILE* f = fopen(fontPath, "rb");
    if (!f) { perror("fopen"); return 2; }
    fseek(f, 0, SEEK_END);
    long fontSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> fontBuffer(fontSize);
    if (fread(fontBuffer.data(), 1, fontSize, f) != (size_t)fontSize) {
        perror("fread");
        fclose(f);
        return 3;
    }
    fclose(f);

    // Initialize font
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontBuffer.data(), 0)) {
        fprintf(stderr, "Failed to init font\n");
        return 4;
    }

    // Create RGBA buffer initialized transparent
    int W = size, H = size;
    std::vector<unsigned char> img(W * H * 4);
    memset(img.data(), 0, img.size()); // alpha = 0 => transparent

    // Choose a scale that fits the text into the square
    // We'll measure the text width at a nominal pixel height and scale to fit.
    float nominalPixelHeight = 64.0f;
    float scale = stbtt_ScaleForPixelHeight(&font, nominalPixelHeight);

    // Measure width and ascent/descent
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    float baseline = ascent * scale;

    // Compute width of the text in pixels at nominal scale
    int totalWidth = 0;
    for (const char* p = text; *p; ++p) {
        int ax;
        int lsb;
        stbtt_GetCodepointHMetrics(&font, (int)(unsigned char)*p, &ax, &lsb);
        totalWidth += (int)floor(ax * scale + 0.5f);
        // kerning
        if (*(p+1)) {
            totalWidth += (int)floor(stbtt_GetCodepointKernAdvance(&font, (int)(unsigned char)*p, (int)(unsigned char)*(p+1)) * scale + 0.5f);
        }
    }

    // Determine final scale to fit into W (leave small margin)
    float margin = 0.05f * W;
    float targetWidth = W - 2.0f * margin;
    float finalScale = scale * (targetWidth / (float)totalWidth);

    // Recompute baseline for finalScale
    float finalBaseline = ascent * finalScale;

    // Render each glyph into the image buffer
    // We'll render into a temporary grayscale bitmap for each glyph and blend into RGBA
    int penX = (int)floor((W - (totalWidth * (finalScale/scale))) / 2.0f + 0.5f); // center horizontally
    int penY = (int)floor(finalBaseline + (H - (ascent - descent) * finalScale) / 2.0f + 0.5f); // center vertically

    for (const char* p = text; *p; ++p) {
        int codepoint = (unsigned char)*p;
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &lsb);
        int kern = 0;
        if (*(p+1)) kern = stbtt_GetCodepointKernAdvance(&font, codepoint, (unsigned char)*(p+1));

        int glyphW, glyphH, xoff, yoff;
        unsigned char* glyphBitmap = stbtt_GetCodepointBitmap(&font, 0, finalScale, codepoint, &glyphW, &glyphH, &xoff, &yoff);
        if (glyphBitmap) {
            // Blend glyphBitmap into img at (penX + xoff, penY + yoff)
            int dstX = penX + xoff;
            int dstY = penY + yoff;
            for (int gy = 0; gy < glyphH; ++gy) {
                int iy = dstY + gy;
                if (iy < 0 || iy >= H) continue;
                for (int gx = 0; gx < glyphW; ++gx) {
                    int ix = dstX + gx;
                    if (ix < 0 || ix >= W) continue;
                    unsigned char srcA = glyphBitmap[gy * glyphW + gx];
                    if (srcA == 0) continue;
                    int idx = (iy * W + ix) * 4;
                    // We'll render white text; change RGB here if needed.
                    // Alpha blending: outA = srcA + dstA*(1-srcA)
                    unsigned char dstA = img[idx + 3];
                    int outA = srcA + (dstA * (255 - srcA) + 127) / 255;
                    // Pre-multiplied blending for white color (255,255,255)
                    int dstR = img[idx + 0];
                    int dstG = img[idx + 1];
                    int dstB = img[idx + 2];
                    int outR = (srcA * 255 + dstR * dstA * (255 - srcA) / 255) / (outA ? outA : 1);
                    int outG = (srcA * 255 + dstG * dstA * (255 - srcA) / 255) / (outA ? outA : 1);
                    int outB = (srcA * 255 + dstB * dstA * (255 - srcA) / 255) / (outA ? outA : 1);
                    img[idx + 0] = clamp255(outR);
                    img[idx + 1] = clamp255(outG);
                    img[idx + 2] = clamp255(outB);
                    img[idx + 3] = clamp255(outA);
                }
            }
            stbtt_FreeBitmap(glyphBitmap, nullptr);
        }
        penX += (int)floor(advance * finalScale + 0.5f);
        penX += (int)floor(kern * finalScale + 0.5f);
    }

    // Write PNG (RGBA)
    if (!stbi_write_png(outPath, W, H, 4, img.data(), W * 4)) {
        fprintf(stderr, "Failed to write PNG\n");
        return 5;
    }

    printf("Wrote %s (%dx%d)\n", outPath, W, H);
    return 0;
}