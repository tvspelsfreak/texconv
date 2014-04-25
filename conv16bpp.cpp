#include "imagecontainer.h"
#include "twiddler.h"
#include "vqtools.h"

#include <QFile>
#include <QDebug>

void convertAndWriteTexel(QDataStream& stream, const QRgb& texel, int pixelFormat, bool twiddled);
void writeStrideData(QDataStream& stream, const QImage& img, int pixelFormat);
void writeUncompressedData(QDataStream& stream, const ImageContainer& images, int pixelFormat);
void writeCompressedData(QDataStream& stream, const ImageContainer& images, int pixelFormat);

void convert16BPP(QDataStream& stream, const ImageContainer& images, int textureType) {
	const int pixelFormat = (textureType >> PIXELFORMAT_SHIFT) & PIXELFORMAT_MASK;

	if (textureType & FLAG_STRIDED) {
		writeStrideData(stream, images.getByIndex(0), pixelFormat);
	} else if (textureType & FLAG_COMPRESSED) {
		writeCompressedData(stream, images, pixelFormat);
	} else {
		writeUncompressedData(stream, images, pixelFormat);
	}
}


void convertAndWriteTexel(QDataStream& stream, const QRgb& texel, int pixelFormat, bool twiddled) {
	if (pixelFormat == PIXELFORMAT_YUV422) {
		static int index = 0;
		static QRgb savedTexel[3];

		if (!twiddled && index == 1) {
			quint16 yuv[2];
			RGBtoYUV422(savedTexel[0], texel, yuv[0], yuv[1]);
			stream << yuv[0];
			stream << yuv[1];
			index = 0;
		} else if (twiddled && index == 3) {
			quint16 yuv[4];
			RGBtoYUV422(savedTexel[0], savedTexel[2], yuv[0], yuv[2]);
			RGBtoYUV422(savedTexel[1],         texel, yuv[1], yuv[3]);
			stream << yuv[0];
			stream << yuv[1];
			stream << yuv[2];
			stream << yuv[3];
			index = 0;
		} else {
			savedTexel[index] = texel;
			index++;
		}
	} else {
		stream << to16BPP(texel, pixelFormat);
	}
}

void writeStrideData(QDataStream& stream, const QImage& img, int pixelFormat) {
	for (int y=0; y<img.height(); y++)
		for (int x=0; x<img.width(); x++)
			convertAndWriteTexel(stream, img.pixel(x, y), pixelFormat, false);
}

void writeUncompressedData(QDataStream& stream, const ImageContainer& images, int pixelFormat) {
	// Mipmap offset
	if (images.hasMipmaps()) {
		writeZeroes(stream, MIPMAP_OFFSET_16BPP);
	}

	// Texture data, from smallest to largest mipmap
	for (int i=0; i<images.imageCount(); i++) {
		const QImage& img = images.getByIndex(i);

		// The 1x1 mipmap level is a bit special for YUV textures. Since there's only
		// one pixel, it can't be saved as YUV422, so save it as RGB565 instead.
		if (img.width() == 1 && img.height() == 1 && pixelFormat == PIXELFORMAT_YUV422) {
			convertAndWriteTexel(stream, img.pixel(0, 0), PIXELFORMAT_RGB565, true);
			continue;
		}

		const Twiddler twiddler(img.width(), img.height());
		const int pixels = img.width() * img.height();

		// Write all texels for this mipmap level in twiddled order
		for (int j=0; j<pixels; j++) {
			const int index = twiddler.index(j);
			const int x = index % img.width();
			const int y = index / img.width();
			convertAndWriteTexel(stream, img.pixel(x, y), pixelFormat, true);
		}
	}
}

// Packs a quad (2x2 16BPP texels) into a single quint64
static quint64 packQuad(QRgb topLeft, QRgb topRight, QRgb bottomLeft, QRgb bottomRight, int pixelFormat) {
	quint64 a, b, c, d;
	if (pixelFormat == PIXELFORMAT_YUV422) {
		quint16 yuv[4];
		RGBtoYUV422(topLeft,    topRight,    yuv[0], yuv[1]);
		RGBtoYUV422(bottomLeft, bottomRight, yuv[2], yuv[3]);
		a = yuv[0];
		b = yuv[1];
		c = yuv[2];
		d = yuv[3];
	} else {
		a = to16BPP(topLeft,     pixelFormat);
		b = to16BPP(topRight,    pixelFormat);
		c = to16BPP(bottomLeft,  pixelFormat);
		d = to16BPP(bottomRight, pixelFormat);
	}
	return (a << 48) | (b << 32) | (c << 16) | d;
}


// This function counts how many unique 2x2 16BPP pixel blocks there are in the image.
// If there are <= maxCodes, it puts the unique blocks in 'codebook' and 'indexedImages'
// will contain images that index the 'codebook' vector, resulting in quick "lossless"
// compression, if possible.
// It will keep counting blocks even if the block count exceeds maxCodes for the sole
// purpose of reporting it back to the user.
// Returns number of unique 2x2 16BPP pixel blocks in all images.
static int encodeLossless(const ImageContainer& images, int pixelFormat, QVector<QImage>& indexedImages, QVector<quint64>& codebook, int maxCodes) {
	QHash<quint64, int> uniqueQuads; // Quad <=> index

	for (int i=0; i<images.imageCount(); i++) {
		const QImage& img = images.getByIndex(i);

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_VQ || img.height() < MIN_MIPMAP_VQ)
			continue;

		QImage indexedImage(img.width() / 2, img.height() / 2, QImage::Format_ARGB32);

		for (int y=0; y<img.height(); y+=2) {
			for (int x=0; x<img.width(); x+=2) {
				QRgb tl = img.pixel(x + 0, y + 0);
				QRgb tr = img.pixel(x + 1, y + 0);
				QRgb bl = img.pixel(x + 0, y + 1);
				QRgb br = img.pixel(x + 1, y + 1);
				quint64 quad = packQuad(tl, tr, bl, br, pixelFormat);

				if (!uniqueQuads.contains(quad))
					uniqueQuads.insert(quad, uniqueQuads.size());

				if (uniqueQuads.size() <= maxCodes)
					indexedImage.setPixel(x / 2, y / 2, uniqueQuads.value(quad));
			}
		}

		// Only add the image if we haven't hit the code limit
		if (uniqueQuads.size() <= maxCodes) {
			indexedImages.push_back(indexedImage);
		}
	}

	if (uniqueQuads.size() <= maxCodes) {
		// This texture can be losslessly compressed.
		// Copy the unique quads over to the codebook.
		// indexedImages is already done.
		codebook.resize(uniqueQuads.size());
		for (auto it = uniqueQuads.cbegin(); it != uniqueQuads.cend(); ++it)
			codebook[it.value()] = it.key();
	} else {
		// This texture needs lossy compression
		indexedImages.clear();
	}

	return uniqueQuads.size();
}

// Divides the image into 2x2 pixel blocks and stores them as 12-dimensional
// vectors, (R, G, B) * 4.
static void vectorizeRGB(const ImageContainer& images, QVector<Vec<12>>& vectors) {
	for (int i=0; i<images.imageCount(); i++) {
		const QImage& img = images.getByIndex(i);

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_VQ || img.height() < MIN_MIPMAP_VQ)
			continue;

		for (int y=0; y<img.height(); y+=2) {
			for (int x=0; x<img.width(); x+=2) {
				Vec<12> vec;
				uint hash = 0;
				int offset = 0;
				for (int yy=y; yy<(y+2); yy++) {
					for (int xx=x; xx<(x+2); xx++) {
						QRgb pixel = img.pixel(xx, yy);
						rgb2vec(pixel, vec, offset);
						hash = combineHash(pixel, hash);
						offset += 3;
					}
				}
				vec.setHash(hash);
				vectors.push_back(vec);
			}
		}
	}
}

// Divides the image into 2x2 pixel blocks and stores them as 16-dimensional
// vectors, (A, R, G, B) * 4.
static void vectorizeARGB(const ImageContainer& images, QVector<Vec<16>>& vectors) {
	for (int i=0; i<images.imageCount(); i++) {
		const QImage& img = images.getByIndex(i);

		// Ignore images smaller than this
		if (img.width() < MIN_MIPMAP_VQ || img.height() < MIN_MIPMAP_VQ)
			continue;

		for (int y=0; y<img.height(); y+=2) {
			for (int x=0; x<img.width(); x+=2) {
				Vec<16> vec;
				uint hash = 0;
				int offset = 0;
				for (int yy=y; yy<(y+2); yy++) {
					for (int xx=x; xx<(x+2); xx++) {
						QRgb pixel = img.pixel(xx, yy);
						argb2vec(pixel, vec, offset);
						hash = combineHash(pixel, hash);
						offset += 4;
					}
				}
				vec.setHash(hash);
				vectors.push_back(vec);
			}
		}
	}
}

static void devectorizeRGB(const ImageContainer& srcImages, const QVector<Vec<12>>& vectors, const VectorQuantizer<12>& vq, int pixelFormat, QVector<QImage>& indexedImages, QVector<quint64>& codebook) {
	int vindex = 0;

	for (int i=0; i<srcImages.imageCount(); i++) {
		int size = srcImages.getByIndex(i).width();
		if (size == 1)
			continue;
		QImage img(size/2, size/2, QImage::Format_Indexed8);
		img.setColorCount(256);
		for (int y=0; y<img.height(); y++) {
			for (int x=0; x<img.width(); x++) {
				const Vec<12>& vec = vectors[vindex];
				int codeIndex = vq.findClosest(vec);
				img.setPixel(x, y, codeIndex);
				vindex++;
			}
		}
		indexedImages.push_back(img);
	}

	for (int i=0; i<vq.codeCount(); i++) {
		const Vec<12>& vec = vq.codeVector(i);
		QColor tl = QColor::fromRgbF(vec[0], vec[1], vec[2]);
		QColor tr = QColor::fromRgbF(vec[3], vec[4], vec[5]);
		QColor bl = QColor::fromRgbF(vec[6], vec[7], vec[8]);
		QColor br = QColor::fromRgbF(vec[9], vec[10], vec[11]);
		quint64 quad = packQuad(tl.rgb(), tr.rgb(), bl.rgb(), br.rgb(), pixelFormat);
		codebook.push_back(quad);
	}
}

static void devectorizeARGB(const ImageContainer& srcImages, const QVector<Vec<16>>& vectors, const VectorQuantizer<16>& vq, int format, QVector<QImage>& indexedImages, QVector<quint64>& codebook) {
	int vindex = 0;

	for (int i=0; i<srcImages.imageCount(); i++) {
		int size = srcImages.getByIndex(i).width();
		if (size == 1)
			continue;
		QImage img(size/2, size/2, QImage::Format_Indexed8);
		img.setColorCount(256);
		for (int y=0; y<img.height(); y++) {
			for (int x=0; x<img.width(); x++) {
				const Vec<16>& vec = vectors[vindex];
				int codeIndex = vq.findClosest(vec);
				img.setPixel(x, y, codeIndex);
				vindex++;
			}
		}
		indexedImages.push_back(img);
	}

	for (int i=0; i<vq.codeCount(); i++) {
		const Vec<16>& vec = vq.codeVector(i);
		QColor tl = QColor::fromRgbF(vec[1], vec[2], vec[3], vec[0]);
		QColor tr = QColor::fromRgbF(vec[5], vec[6], vec[7], vec[4]);
		QColor bl = QColor::fromRgbF(vec[9], vec[10], vec[11], vec[8]);
		QColor br = QColor::fromRgbF(vec[13], vec[14], vec[15], vec[12]);
		quint64 quad = packQuad(tl.rgba(), tr.rgba(), bl.rgba(), br.rgba(), format);
		codebook.push_back(quad);
	}
}

void writeCompressedData(QDataStream& stream, const ImageContainer& images, int pixelFormat) {
	QVector<QImage> indexedImages;
	QVector<quint64> codebook;

	const int numQuads = encodeLossless(images, pixelFormat, indexedImages, codebook, 256);

	qDebug() << "Source images contain" << numQuads << "unique quads";

	if (numQuads > 256) {
		if ((pixelFormat != PIXELFORMAT_ARGB1555) && (pixelFormat != PIXELFORMAT_ARGB4444)) {
			QVector<Vec<12>> vectors;
			VectorQuantizer<12> vq;
			vectorizeRGB(images, vectors);
			vq.compress(vectors, 256);
			devectorizeRGB(images, vectors, vq, pixelFormat, indexedImages, codebook);
		} else {
			QVector<Vec<16>> vectors;
			VectorQuantizer<16> vq;
			vectorizeARGB(images, vectors);
			vq.compress(vectors, 256);
			devectorizeARGB(images, vectors, vq, pixelFormat, indexedImages, codebook);
		}
	}

	// Build the codebook
	quint16 codes[256 * 4];
	memset(codes, 0, 2048);
	for (int i=0; i<codebook.size(); i++) {
		const quint64& quad = codebook[i];
		codes[i * 4 + 0] = (quint16)((quad >> 48) & 0xFFFF);
		codes[i * 4 + 1] = (quint16)((quad >> 16) & 0xFFFF);
		codes[i * 4 + 2] = (quint16)((quad >> 32) & 0xFFFF);
		codes[i * 4 + 3] = (quint16)((quad >>  0) & 0xFFFF);
	}

	// Write the codebook
	for (int i=0; i<1024; i++)
		stream << codes[i];

	// Write the 1x1 mipmap level
	if (images.imageCount() > 1)
		writeZeroes(stream, 1);

	// Write all mipmap levels
	for (int i=0; i<indexedImages.size(); i++) {
		const QImage& img = indexedImages[i];
		const Twiddler twiddler(img.width(), img.height());
		const int pixels = img.width() * img.height();

		for (int j=0; j<pixels; j++) {
			const int index = twiddler.index(j);
			const int x = index % img.width();
			const int y = index / img.width();
			stream << (quint8)img.pixelIndex(x, y);
		}
	}
}
