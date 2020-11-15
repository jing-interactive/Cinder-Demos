// Copyright (c) 2011-2018 Sundog Software LLC. All rights reserved worldwide.

#include "TGALoader.h"
#include  "../../../Triton SDK/Public Headers/ResourceLoader.h"

#include <cstdio>

typedef struct {
    unsigned char ImgIdent;
    unsigned char ignored[ 1 ];
    unsigned char ImgType;
    unsigned char ignored2[ 9 ];
    unsigned char WidthLo;
    unsigned char WidthHi;
    unsigned char HeightLo;
    unsigned char HeightHi;
    unsigned char Bpp;
    unsigned char ignored3[ 1 ];
} TGAHeader;

TGALoader::TGALoader() : pixels(0)
{
}

TGALoader::~TGALoader()
{
    if (pixels) {
        free(pixels);
    }
}

bool TGALoader::Load(const char *filename)
{
    FILE *f = NULL;
#ifdef _WIN32
#if _MSC_VER >= 1400
    fopen_s(&f, filename, "rb");
#else
    f = fopen(filename, "rb");
#endif
#else
    f = fopen(filename, "rb");
#endif

    if (f) {
        TGAHeader header;
        size_t ret = fread(&header, 1, sizeof(TGAHeader), f);

        // Precalc some values from the header
        const unsigned int imageType  = header.ImgType;
        const unsigned int imageWidth  = header.WidthLo  + header.WidthHi  * 256;
        const unsigned int imageHeight  = header.HeightLo + header.HeightHi * 256;
        const unsigned int imageBytesPerPel = header.Bpp / 8;
        const unsigned int imageSize  = imageWidth * imageHeight * imageBytesPerPel;

        width = imageWidth;
        height = imageHeight;
        bytesPerPixel = imageBytesPerPel;

        // Validate header info
        if ( ( imageType != 2 && imageType != 3 && imageType != 10 ) ||
                ( imageWidth == 0 ) || ( imageHeight == 0 ) ||
                ( imageBytesPerPel != 3 && imageBytesPerPel != 4 && imageBytesPerPel != 1) ) {
            // invalid header, bomb out
            fclose(f);
            return false;
        }

        // Allocate the memory for the image size
        pixels = (unsigned char *)malloc(imageSize);

        // Skip image ident field
        if ( header.ImgIdent > 0 ) {
            fseek(f, header.ImgIdent, SEEK_CUR);
        }

        // un-compresses image ?
        if (imageType == 2 || imageType == 3) {
            size_t ret = fread(pixels, 1, imageSize, f);

            if (imageType == 2) {
                for (unsigned int cswap = 0; cswap < imageSize; cswap += imageBytesPerPel) {
                    pixels[cswap] ^= pixels[cswap+2];
                    pixels[cswap+2] ^= pixels[cswap];
                    pixels[cswap] ^= pixels[cswap+2];
                }
            }
        }

        fclose(f);

        return true;
    }

    return false;
}

void TGALoader::flipVertical()
{
    for (int row = 0; row < height/2; ++row) {
        for (int col = 0; col < width; ++col) {
            for (int byte = 0; byte < bytesPerPixel; ++byte) {
                const int pos1 = (row*width + col)*bytesPerPixel + byte;
                const int pos2 = ((height - row - 1)*width + col)*bytesPerPixel + byte;

                unsigned char temp = pixels[pos1];
                pixels[pos1] = pixels[pos2];
                pixels[pos2] = temp;
            }
        }
    }
}
void TGALoader::flipHorizontal()
{
    for (int col = 0; col < width/2; ++col) {
        for (int row = 0; row < height; ++row) {
            for (int byte = 0; byte < bytesPerPixel; ++byte) {
                const int pos1 = (row*width + col)*bytesPerPixel + byte;
                const int pos2 = (row*width + (height - col - 1))*bytesPerPixel + byte;

                unsigned char temp = pixels[pos1];
                pixels[pos1] = pixels[pos2];
                pixels[pos2] = temp;
            }
        }
    }
}

void TGALoader::FreePixels(void)
{
    if (pixels) {
        free(pixels);
        pixels = 0;
    }
}