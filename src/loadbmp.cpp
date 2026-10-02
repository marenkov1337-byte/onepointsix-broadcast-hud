#include "stdafx.h"

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t signature;
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
};
struct BmpInfoHeader {
    uint32_t headerSize;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};
struct BmpPaletteEntry {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char reserved;
};
#pragma pack(pop)

auto load_bmp_texture(const char *filename, int *width, int *height) -> unsigned int {
    FILE *fp = fopen(filename, "rb");
    if (fp == nullptr) return 0;

    BmpFileHeader fileHeader{};
    BmpInfoHeader infoHeader{};
    if (fread(&fileHeader, sizeof(fileHeader), 1, fp) != 1 || fileHeader.signature != 0x4D42) {
        fclose(fp);
        return 0;
    }
    if (fread(&infoHeader, sizeof(infoHeader), 1, fp) != 1 || infoHeader.headerSize < 40) {
        fclose(fp);
        return 0;
    }

    // Stock CS 1.6 overviews are commonly uncompressed 8-bit paletted BMPs.
    // Keep support for the 24/32-bit overviews already handled by the DLL.
    if (infoHeader.compression != 0 ||
        (infoHeader.bitCount != 8 && infoHeader.bitCount != 24 && infoHeader.bitCount != 32) ||
        infoHeader.width <= 0 || infoHeader.height == 0) {
        fclose(fp);
        return 0;
    }

    const int w = infoHeader.width;
    const bool topDown = infoHeader.height < 0;
    const int h = topDown ? -infoHeader.height : infoHeader.height;

    std::vector<BmpPaletteEntry> palette;
    if (infoHeader.bitCount == 8) {
        uint32_t paletteCount = infoHeader.colorsUsed != 0 ? infoHeader.colorsUsed : 256;
        if (paletteCount > 256) paletteCount = 256;
        palette.resize(paletteCount);
        // Palette follows the complete DIB header, not necessarily our 40-byte struct.
        if (fseek(fp, static_cast<long>(sizeof(BmpFileHeader) + infoHeader.headerSize), SEEK_SET) != 0 ||
            fread(palette.data(), sizeof(BmpPaletteEntry), paletteCount, fp) != paletteCount) {
            fclose(fp);
            return 0;
        }
    }

    const int bitsPerRow = w * infoHeader.bitCount;
    const int rowSize = ((bitsPerRow + 31) / 32) * 4;
    std::vector<unsigned char> raw(static_cast<size_t>(rowSize) * h);
    if (fseek(fp, static_cast<long>(fileHeader.dataOffset), SEEK_SET) != 0 ||
        fread(raw.data(), 1, raw.size(), fp) != raw.size()) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const int srcRow = topDown ? y : (h - 1 - y);
        const unsigned char *src = &raw[static_cast<size_t>(srcRow) * rowSize];
        unsigned char *dst = &rgba[static_cast<size_t>(y) * w * 4];

        for (int x = 0; x < w; ++x) {
            if (infoHeader.bitCount == 8) {
                const unsigned int index = src[x];
                if (index >= palette.size()) return 0;
                const BmpPaletteEntry &c = palette[index];
                dst[0] = c.r;
                dst[1] = c.g;
                dst[2] = c.b;
                dst[3] = 255;
            } else {
                const int bytesPerPixel = infoHeader.bitCount / 8;
                const unsigned char *pixel = src + x * bytesPerPixel;
                dst[0] = pixel[2];
                dst[1] = pixel[1];
                dst[2] = pixel[0];
                dst[3] = (bytesPerPixel == 4) ? pixel[3] : 255;
            }

            // GoldSrc overview BMPs use bright green as the transparent
            // background/chroma key.  This is especially common on the stock
            // 8-bit paletted overviews (Inferno, Dust2, etc.).  Clear both
            // alpha and RGB so GL_LINEAR filtering cannot bleed green around
            // the map edges.
            if (dst[1] >= 248 && dst[0] <= 8 && dst[2] <= 8) {
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = 0;
                dst[3] = 0;
            }

            dst += 4;
        }
    }

    if (width != nullptr) *width = w;
    if (height != nullptr) *height = h;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}
