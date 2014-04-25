#ifndef COMMON_H
#define COMMON_H

#include <QColor>

class ImageContainer;
class QDataStream;

#define PIXELFORMAT_ARGB1555	0
#define PIXELFORMAT_RGB565		1
#define PIXELFORMAT_ARGB4444	2
#define PIXELFORMAT_YUV422		3
#define PIXELFORMAT_BUMPMAP		4
#define PIXELFORMAT_PAL4BPP		5
#define PIXELFORMAT_PAL8BPP		6
#define PIXELFORMAT_MASK		7
#define PIXELFORMAT_SHIFT		27

#define FLAG_NONTWIDDLED		(1 << 26)
#define FLAG_STRIDED			(1 << 25)
#define FLAG_COMPRESSED			(1 << 30)
#define FLAG_MIPMAPPED			(1 << 31)

// Min/max size supported by the PVR2DC
#define TEXTURE_SIZE_MIN	8
#define TEXTURE_SIZE_MAX	1024
#define TEXTURE_STRIDE_MIN	32
#define TEXTURE_STRIDE_MAX	992

// Minimum mipmap sizes
#define MIN_MIPMAP_VQ		2
#define MIN_MIPMAP_PALVQ	4

// Magic identifiers
#define TEXTURE_MAGIC		"DTEX"
#define PALETTE_MAGIC		"DPAL"

// Mipmapped uncompressed textures all have a small offset
// before the actual texture data starts.
#define MIPMAP_OFFSET_4BPP  1
#define MIPMAP_OFFSET_8BPP  3
#define MIPMAP_OFFSET_16BPP 6

// Returns the nearest higher or equal power of two to x.
int nextPowerOfTwo(int x);

// Returns true if the texture size is valid on dreamcast.
bool isValidSize(int width, int height, int textureType);

// Writes n bytes of zeroes to the stream
void writeZeroes(QDataStream& stream, int n);

bool isFormat(int textureType, int pixelFormat);
bool isPaletted(int textureType);
bool is16BPP(int textureType);

// Texel conversion
quint16	to16BPP(QRgb argb, int pixelFormat);
QRgb	to32BPP(quint16 argb, int pixelFormat);

void RGBtoYUV422(const QRgb rgb1, const QRgb rgb2, quint16& yuv1, quint16& yuv2);
void YUV422toRGB(const quint16 yuv1, const quint16 yuv2, QRgb& rgb1, QRgb& rgb2);

int writeTextureHeader(QDataStream& stream, int width, int height, int textureType);

// Taken from boost. This increases hash performance by A LOT compared to just
// xor-ing the rgba values together. I've observed everything from 30x to 320x faster.
uint combineHash(const QRgb& rgba, uint seed);

// conv16bpp.cpp
void convert16BPP(QDataStream& stream, const ImageContainer& images, int textureType);

// convpal.cpp
void convertPaletted(QDataStream& stream, const ImageContainer& images, int textureType, const QString& paletteFilename);

// preview.cpp
bool generatePreview(const QString& textureFilename, const QString& paletteFilename, const QString& previewFilename, const QString& codeUsageFilename);

#endif // COMMON_H
