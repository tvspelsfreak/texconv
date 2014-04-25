#include "common.h"
#include "imagecontainer.h"
#include "vqtools.h"
#include <assert.h>

static inline bool powerOfTwo(int x) {
	return ((x != 0) && !(x & (x - 1)));
}

int nextPowerOfTwo(int x) {
	if (x <= 0)
		return 1;

	int pw2 = 1;
	while (pw2 < x)
		pw2 *= 2;

	return pw2;
}

bool isValidSize(int width, int height, int textureType) {
	if (textureType & FLAG_STRIDED) {
		if (width < TEXTURE_STRIDE_MIN || width > TEXTURE_STRIDE_MAX || (width % 32) != 0)
			return false;
		if (height < TEXTURE_SIZE_MIN || height > TEXTURE_SIZE_MAX || !powerOfTwo(height))
			return false;
	} else {
		// Allow the user to supply textures down to 1x1 if we're doing mipmaps.
		const int MINSIZE = (textureType & FLAG_MIPMAPPED) ? 1 : TEXTURE_SIZE_MIN;
		if (width < MINSIZE || width > TEXTURE_SIZE_MAX || !powerOfTwo(width))
			return false;
		if (height < MINSIZE || height > TEXTURE_SIZE_MAX || !powerOfTwo(height))
			return false;
	}
	return true;
}

void writeZeroes(QDataStream& stream, int n) {
	quint8 zero = 0;
	for (int i=0; i<n; i++)
		stream << zero;
}


bool isFormat(int textureType, int pixelFormat) {
	return ((textureType >> PIXELFORMAT_SHIFT) & PIXELFORMAT_MASK) == pixelFormat;
}

bool isPaletted(int textureType) {
	return isFormat(textureType, PIXELFORMAT_PAL4BPP) || isFormat(textureType, PIXELFORMAT_PAL8BPP);
}

bool is16BPP(int textureType) {
	return !isPaletted(textureType);
}



#define DOUBLEPI	(M_PI * 2.0)
#define HALFPI		(M_PI / 2.0)

static quint16 toSpherical(QRgb color) {
	QColor c(color);
	Vec<3> cartesian;
	cartesian.set(0, c.redF() * 2.0f - 1.0f);
	cartesian.set(1, c.greenF() * 2.0f - 1.0f);
	cartesian.set(2, c.blueF()/* * 2.0f - 1.0f*/);

	float radius = cartesian.length();
	float polar = acos(cartesian[2] / radius);
	float azimuth = atan2(cartesian[1], cartesian[0]);

	// The polar angle is 0 to PI where 0 would mean a vector pointing straight
	// up and PI is a vector pointing straight down. We need to convert this to:
	// 0 = flat, 255 = straight up.
	polar = HALFPI - polar;				// -HALFPI ... HALFPI
	polar = (polar / HALFPI) * 255.0;	// -255 ... 255
	int S = qBound(0, (int)polar, 255);	// 0 ... 255

	// The azimuthal angle is -PI to PI and we need to convert it to 0 to 255.
	if (azimuth < 0) azimuth += DOUBLEPI;	// 0 ... DOUBLEPI
	azimuth = (azimuth / DOUBLEPI) * 255.0;	// 0 ... 255
	int R = qBound(0, (int)azimuth, 255);

	// Return the two values packed together into one texel
	return (quint16)((S << 8) | R);
}

static QRgb toCartesian(quint16 SR) {
	float S = (1.0 - ((SR >> 8) / 255.0)) * HALFPI;
	float R = ((SR & 0xFF) / 255.0) * DOUBLEPI;
	if (R > M_PI) R -= DOUBLEPI;
	QColor color = QColor::fromRgbF(
				(sin(S) * cos(R) + 1.0f) * 0.5f,
				(sin(S) * sin(R) + 1.0f) * 0.5f,
				(cos(S) + 1.0f) * 0.5f);
	return color.rgb();
}

quint16 to16BPP(QRgb argb, int pixelFormat) {
	quint16 a, r, g, b;
	switch (pixelFormat) {
	case PIXELFORMAT_ARGB1555:
		a = (qAlpha(argb) < 128) ? 0 : 1;
		r = (quint16)((qRed(argb)   >> 3) & 0x1F);
		g = (quint16)((qGreen(argb) >> 3) & 0x1F);
		b = (quint16)((qBlue(argb)  >> 3) & 0x1F);
		return (a << 15) | (r << 10) | (g << 5) | b;
	case PIXELFORMAT_RGB565:
		r = (quint16)((qRed(argb)   >> 3) & 0x1F);
		g = (quint16)((qGreen(argb) >> 2) & 0x3F);
		b = (quint16)((qBlue(argb)  >> 3) & 0x1F);
		return (r << 11) | (g << 5) | b;
	case PIXELFORMAT_ARGB4444:
		a = (quint16)((qAlpha(argb) >> 4) & 0xF);
		r = (quint16)((qRed(argb)   >> 4) & 0xF);
		g = (quint16)((qGreen(argb) >> 4) & 0xF);
		b = (quint16)((qBlue(argb)  >> 4) & 0xF);
		return (a << 12) | (r << 8) | (g << 4) | b;
	case PIXELFORMAT_BUMPMAP:
		return toSpherical(argb);
	default:
		qCritical() << "Unsupported format" << pixelFormat << "in to16BPP";
		return 0xFFFF;
	}
}

QRgb to32BPP(quint16 argb, int pixelFormat) {
	int a, r, g, b;
	switch (pixelFormat) {
	case PIXELFORMAT_ARGB1555:
		a = ((argb >> 15) == 1) ? 255 : 0;
		r = ((argb >> 10) & 0x1F) << 3;
		g = ((argb >>  5) & 0x1F) << 3;
		b = ((argb >>  0) & 0x1F) << 3;
		return qRgba(r, g, b, a);
	case PIXELFORMAT_RGB565:
		r = ((argb >> 11) & 0x1F) << 3;
		g = ((argb >>  5) & 0x3F) << 2;
		b = ((argb >>  0) & 0x1F) << 3;
		return qRgb(r, g, b);
	case PIXELFORMAT_ARGB4444:
		a = ((argb >> 12) & 0xF) << 4;
		r = ((argb >>  8) & 0xF) << 4;
		g = ((argb >>  4) & 0xF) << 4;
		b = ((argb >>  0) & 0xF) << 4;
		return qRgba(r, g, b, a);
	case PIXELFORMAT_BUMPMAP:
		return toCartesian(argb);
	default:
		qCritical() << "Unsupported format" << pixelFormat << "in to32BPP";
		return qRgb(255, 255, 255);
	}
}

void RGBtoYUV422(const QRgb rgb1, const QRgb rgb2, quint16& yuv1, quint16& yuv2) {
	const int avgR = (qRed(rgb1) + qRed(rgb2)) / 2;
	const int avgG = (qGreen(rgb1) + qGreen(rgb2)) / 2;
	const int avgB = (qBlue(rgb1) + qBlue(rgb2)) / 2;

	//compute each pixel's Y
	int Y0 = qBound(0, (int)(0.299 * qRed(rgb1) + 0.587 * qGreen(rgb1) + 0.114 * qBlue(rgb1)), 255);
	int Y1 = qBound(0, (int)(0.299 * qRed(rgb2) + 0.587 * qGreen(rgb2) + 0.114 * qBlue(rgb2)), 255);

	//compute UV
//	int U = qBound(0, (int)(128.0 - 0.14 * avgR - 0.29 * avgG + 0.43 * avgB), 255);
//	int V = qBound(0, (int)(128.0 + 0.36 * avgR - 0.29 * avgG - 0.07 * avgB), 255);

	int U = qBound(0, (int)(-0.169 * avgR - 0.331 * avgG + 0.4990 * avgB + 128), 255);
	int V = qBound(0, (int)( 0.499 * avgR - 0.418 * avgG - 0.0813 * avgB + 128), 255);

	yuv1 = ((quint16)Y0) << 8 | (quint16)U;
	yuv2 = ((quint16)Y1) << 8 | (quint16)V;
}

void YUV422toRGB(const quint16 yuv1, const quint16 yuv2, QRgb& rgb1, QRgb& rgb2) {
	const int Y0 = (yuv1 & 0xFF00) >> 8;
	const int Y1 = (yuv2 & 0xFF00) >> 8;
	const int U = (int)(yuv1 & 0xFF) - 128;
	const int V = (int)(yuv2 & 0xFF) - 128;
	int r, g, b;

	r = qBound(0, (int)(Y0 + 1.375 * V), 255);
	g = qBound(0, (int)(Y0 - 0.34375 * U - 0.6875 * V), 255);
	b = qBound(0, (int)(Y0 + 1.71875 * U), 255);
	rgb1 = qRgb(r, g, b);

	r = qBound(0, (int)(Y1 + 1.375 * V), 255);
	g = qBound(0, (int)(Y1 - 0.34375 * U - 0.6875 * V), 255);
	b = qBound(0, (int)(Y1 + 1.71875 * U), 255);
	rgb2 = qRgb(r, g, b);
}



// Returns how many pixels a w*h texture contains.
// minw and minh is the size of the smallest mipmap in the texture.
// For textures without mipmaps, set minw=w and minh=h.
static int getPixelCount(int w, int h, int minw, int minh) {
	if ((w < minw) || (h < minh))
		return 0;
	return w * h + getPixelCount(w / 2, h / 2, minw, minh);
}

int calculateSize(int w, int h, int textureType) {
	const bool mipmapped = (textureType & FLAG_MIPMAPPED);
	const bool compressed = (textureType & FLAG_COMPRESSED);
	int bytes = 0;

	if (mipmapped) {
		if (compressed) {
			bytes += 2048;	// Codebook
			bytes += 1;		// The 1x1 mipmap is never used in vq textures
			if (is16BPP(textureType)) {
				// 8x compression
				// The smallest mipmap is 2x2
				bytes += (getPixelCount(w, h, 2, 2) / 4);
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				// 32x compression
				// The smallest mipmap is 4x4
				bytes += (getPixelCount(w, h, 4, 4) / 16);
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				// 16x compression
				// The smallest mipmap is 4x4
				bytes += (getPixelCount(w, h, 4, 4) / 8);	// 16x compression
			}
		} else {
			const int pixels = getPixelCount(w, h, 1, 1);
			if (is16BPP(textureType)) {
				bytes += MIPMAP_OFFSET_16BPP;
				bytes += pixels * 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				bytes += MIPMAP_OFFSET_4BPP;
				bytes += 1; // The 1x1 half-pixel
				bytes += (pixels - 1) / 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				bytes += MIPMAP_OFFSET_8BPP;
				bytes += pixels;
			}
		}
	} else {
		const int pixels = getPixelCount(w, h, w, h);
		if (compressed) {
			bytes += 2048;	// Codebook
			if (is16BPP(textureType)) {
				bytes += pixels / 4;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				bytes += pixels / 16;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				bytes += pixels / 8;
			}
		} else {
			if (is16BPP(textureType)) {
				bytes += pixels * 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
				bytes += pixels / 2;
			} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
				bytes += pixels;
			}
		}
	}

	// Make it a multiple of 32
	if (bytes % 32 == 0) {
		return bytes;
	} else {
		return ((bytes / 32) + 1) * 32;
	}
}

int writeTextureHeader(QDataStream& stream, int width, int height, int textureType) {
	const int size = calculateSize(width, height, textureType);

	// For stride textures, the width set in the strip header must still be a power of two.
	// So we'll store the pow2 width as usual and the actual width in the stride setting.
	// Note that this needs to be done AFTER calculating the texture size.
	if (textureType & FLAG_STRIDED) {
		width = nextPowerOfTwo(width);
	}

	stream.writeRawData(TEXTURE_MAGIC, 4);
	stream << (qint16)width;
	stream << (qint16)height;
	stream << (qint32)textureType;
	stream << (qint32)size;

	assert(stream.device()->pos() == 16);
	return size;
}



uint combineHash(const QRgb& rgba, uint seed) {
	seed ^= qHash(rgba) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	return seed;
}
