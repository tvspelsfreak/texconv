#include <QFile>
#include <QString>
#include <QDebug>
#include <QImage>
#include <QtEndian>
#include "common.h"
#include "twiddler.h"
#include "palette.h"

// A more or less evenly distributed 256-color palette for visualizing compression code
// usage. These should of course be stored as a pregenerated array of QRgb values or
// something, but I'm too lazy to convert it at the moment...
static const char* colorCodes[256] = {
	"#ffffff", "#e3aaaa", "#ffc7c7", "#aac7c7", "#aac7aa", "#aaaae3", "#aaaaff", "#aae3ff",
	"#ffaae3", "#e3ffaa", "#ffffaa", "#ffaaff", "#aaffc7", "#e3c7ff", "#c7aaaa", "#e3e3e3",
	"#aa7171", "#c78e8e", "#718e8e", "#718e71", "#7171aa", "#7171c7", "#71aac7", "#c771aa",
	"#aac771", "#c7c771", "#c771c7", "#71c78e", "#aa8ec7", "#8e7171", "#aaaaaa", "#c7c7c7",
	"#710000", "#8e1c1c", "#381c1c", "#381c00", "#380038", "#380055", "#383855", "#8e0038",
	"#715500", "#8e5500", "#8e0055", "#38551c", "#711c55", "#550000", "#713838", "#8e5555",
	"#aa38aa", "#c755c7", "#7155c7", "#7155aa", "#7138e3", "#7138ff", "#7171ff", "#c738e3",
	"#aa8eaa", "#c78eaa", "#c738ff", "#718ec7", "#aa55ff", "#8e38aa", "#aa71e3", "#c78eff",
	"#38aa38", "#55c755", "#00c755", "#00c738", "#00aa71", "#00aa8e", "#00e38e", "#55aa71",
	"#38ff38", "#55ff38", "#55aa8e", "#00ff55", "#38c78e", "#1caa38", "#38e371", "#55ff8e",
	"#e300aa", "#ff1cc7", "#aa1cc7", "#aa1caa", "#aa00e3", "#aa00ff", "#aa38ff", "#ff00e3",
	"#e355aa", "#ff55aa", "#ff00ff", "#aa55c7", "#e31cff", "#c700aa", "#e338e3", "#ff55ff",
	"#e3aa00", "#ffc71c", "#aac71c", "#aac700", "#aaaa38", "#aaaa55", "#aae355", "#ffaa38",
	"#e3ff00", "#ffff00", "#ffaa55", "#aaff1c", "#e3c755", "#c7aa00", "#e3e338", "#ffff55",
	"#aaaa00", "#c7c71c", "#71c71c", "#71c700", "#71aa38", "#71aa55", "#71e355", "#c7aa38",
	"#aaff00", "#c7ff00", "#c7aa55", "#71ff1c", "#aac755", "#8eaa00", "#aae338", "#c7ff55",
	"#e30071", "#ff1c8e", "#aa1c8e", "#aa1c71", "#aa00aa", "#aa00c7", "#aa38c7", "#ff00aa",
	"#e35571", "#ff5571", "#ff00c7", "#aa558e", "#e31cc7", "#c70071", "#e338aa", "#ff55c7",
	"#3871aa", "#558ec7", "#008ec7", "#008eaa", "#0071e3", "#0071ff", "#00aaff", "#5571e3",
	"#38c7aa", "#55c7aa", "#5571ff", "#00c7c7", "#388eff", "#1c71aa", "#38aae3", "#55c7ff",
	"#3800aa", "#551cc7", "#001cc7", "#001caa", "#0000e3", "#0000ff", "#0038ff", "#5500e3",
	"#3855aa", "#5555aa", "#5500ff", "#0055c7", "#381cff", "#1c00aa", "#3838e3", "#5555ff",
	"#380071", "#551c8e", "#001c8e", "#001c71", "#0000aa", "#0000c7", "#0038c7", "#5500aa",
	"#385571", "#555571", "#5500c7", "#00558e", "#381cc7", "#1c0071", "#3838aa", "#5555c7",
	"#383800", "#55551c", "#00551c", "#005500", "#003838", "#003855", "#007155", "#553838",
	"#388e00", "#558e00", "#553855", "#008e1c", "#385555", "#1c3800", "#387138", "#558e55",
	"#383838", "#555555", "#005555", "#005538", "#003871", "#00388e", "#00718e", "#553871",
	"#388e38", "#558e38", "#55388e", "#008e55", "#38558e", "#1c3838", "#387171", "#558e8e",
	"#e33838", "#ff5555", "#aa5555", "#aa5538", "#aa3871", "#aa388e", "#aa718e", "#ff3871",
	"#e38e38", "#ff8e38", "#ff388e", "#aa8e55", "#e3558e", "#c73838", "#e37171", "#ff8e8e",
	"#aa0000", "#c71c1c", "#711c1c", "#711c00", "#710038", "#710055", "#713855", "#c70038",
	"#aa5500", "#c75500", "#c70055", "#71551c", "#aa1c55", "#8e0000", "#aa3838", "#c75555"
};

static void drawBlock(QImage& img, const int x, const int y, const int w, const int h, const int codebookIndex) {
	const QRgb color = QColor(colorCodes[codebookIndex]).rgb();
	for (int yy=y; yy<(y+h); yy++)
		for (int xx=x; xx<(x+w); xx++)
			img.setPixel(xx, yy, color);
}

static QImage allocatePreview(int w, int h, bool mipmaps) {
	int ww = mipmaps ? (w+w/2) : w;
	QImage img(ww, h, QImage::Format_ARGB32);
	img.fill(Qt::transparent);
	return img;
}

static QPoint nextOffset(const QPoint& offset, const QSize& size) {
	if (offset.x() == 0)
		return QPoint(size.width(), 0);
	else
		return offset + QPoint(0, size.height());
}

bool generatePreview(const QString& textureFilename, const QString& paletteFilename, const QString& previewFilename, const QString& codeUsageFilename) {
	char	magic[4];
	qint16	width, height;
	qint32	textureType;
	qint32	textureSize;
	quint8* data = NULL;

	const bool genPreview = !previewFilename.isEmpty();
	const bool genCodeUsage = !codeUsageFilename.isEmpty();

	if (textureFilename.isEmpty()) {
		qCritical() << "generatePreview requires a texture filename";
		return false;
	}

	if (!genPreview && !genCodeUsage) {
		qCritical() << "generatePreview requires either a preview filename or a code usage filename";
		return false;
	}

	// Open up an input stream to read the texture
	QFile in(textureFilename);
	if (!in.open(QIODevice::ReadOnly)) {
		qCritical() << "Failed to open" << textureFilename;
		return false;
	}
	QDataStream stream(&in);
	stream.setByteOrder(QDataStream::LittleEndian);

	// Read the header
	stream.readRawData(magic, 4);
	stream >> width;
	stream >> height;
	stream >> textureType;
	stream >> textureSize;

	// Verify the header
	if (memcmp(magic, TEXTURE_MAGIC, 4) != 0) {
		qCritical() << textureFilename << "is not a valid texture file";
		in.close();
		return false;
	}

	// Read the texture data and close the stream
	data = new quint8[textureSize];
	stream.readRawData((char*)data, textureSize);
	in.close();

	if (!genPreview && !(textureType & FLAG_COMPRESSED)) {
		qCritical() << "generatePreview was told to only generate code usage, but texture is not compressed";
		return false;
	}

	// Texture width for stride textures are stored in the stride setting, not in
	// the width field. So unpack that if neccessary.
	if (textureType & FLAG_STRIDED) {
		width = (textureType & 31) * 32;
	}

	const int pixelFormat = (textureType >> PIXELFORMAT_SHIFT) & PIXELFORMAT_MASK;
	QVector<QImage> decodedImages;
	QVector<QImage> codeUsageImages;

	/*qDebug() << "Loaded texture" << textureFilename;
	qDebug("Width        : %d", width);
	qDebug("Height       : %d", height);
	qDebug("TextureType  : %08x", textureType);
	qDebug("Pixel format : %d", pixelFormat);
	qDebug("Size (bytes) : %d", textureSize);*/

	if (textureType & FLAG_STRIDED) {
		QImage img(width, height, QImage::Format_ARGB32);
		img.fill(Qt::transparent);

		if (pixelFormat == PIXELFORMAT_YUV422) {
			for (int y=0; y<height; y++) {
				for (int x=0; x<width; x+=2) {
					const int i0 = (y * width + x + 0) * 2;
					const int i1 = (y * width + x + 1) * 2;
					const quint16 p0 = qFromLittleEndian<quint16>(&data[i0]);
					const quint16 p1 = qFromLittleEndian<quint16>(&data[i1]);
					QRgb rgb0, rgb1;
					YUV422toRGB(p0, p1, rgb0, rgb1);
					img.setPixel(x + 0, y, rgb0);
					img.setPixel(x + 1, y, rgb1);
				}
			}
		} else {
			for (int y=0; y<height; y++) {
				for (int x=0; x<width; x++) {
					const int index = (y * width + x) * 2;
					const quint16 pixel = qFromLittleEndian<quint16>(&data[index]);
					img.setPixel(x, y, to32BPP(pixel, pixelFormat));
				}
			}
		}

		decodedImages.push_back(img);
	} else if (is16BPP(textureType) && !(textureType & FLAG_COMPRESSED)) {
		int currentWidth = width;
		int currentHeight = height;
		int offset = 0;

		if (textureType & FLAG_MIPMAPPED) {
			currentWidth = 1;
			currentHeight = 1;
			offset = MIPMAP_OFFSET_16BPP;
		}

		while (currentWidth <= width && currentHeight <= height) {
			QImage img(currentWidth, currentHeight, QImage::Format_ARGB32);
			img.fill(Qt::transparent);
			const Twiddler twiddler(currentWidth, currentHeight);
			const int pixels = currentWidth * currentHeight;

			if (pixelFormat == PIXELFORMAT_YUV422) {
				if (pixels == 1) {
					// The 1x1 mipmap level for YUV textures is stored as RGB565
					const quint16 texel = qFromLittleEndian<quint16>(&data[offset]);
					img.setPixel(0, 0, to32BPP(texel, PIXELFORMAT_RGB565));
				} else {
					for (int i=0; i<pixels; i+=4) {
						quint16 texel[4];
						QRgb pixel[4];

						for (int j=0; j<4; j++)
							texel[j] = qFromLittleEndian<quint16>(&data[offset + (i+j)*2]);

						YUV422toRGB(texel[0], texel[2], pixel[0], pixel[2]);
						YUV422toRGB(texel[1], texel[3], pixel[1], pixel[3]);

						for (int j=0; j<4; j++) {
							const int twidx = twiddler.index(i+j);
							const int x = twidx % currentWidth;
							const int y = twidx / currentWidth;
							img.setPixel(x, y, pixel[j]);
						}
					}
				}
			} else {
				for (int i=0; i<pixels; i++) {
					const quint16 texel = qFromLittleEndian<quint16>(&data[offset + i*2]);
					const QRgb pixel = to32BPP(texel, pixelFormat);
					const int twidx = twiddler.index(i);
					const int x = twidx % currentWidth;
					const int y = twidx / currentWidth;
					img.setPixel(x, y, pixel);
				}
			}

			decodedImages.push_front(img);

			offset += (currentWidth * currentHeight * 2);
			currentWidth *= 2;
			currentHeight *= 2;
		}
	} else if (isPaletted(textureType) && !(textureType & FLAG_COMPRESSED)) {
		if (paletteFilename.isEmpty())
			return false;
		Palette palette;
		if (!palette.load(paletteFilename))
			return false;

		if (isFormat(textureType, PIXELFORMAT_PAL4BPP)) {
			int currentWidth = width;
			int currentHeight = height;
			int offset = 0;

			if (textureType & FLAG_MIPMAPPED) {
				currentWidth = 1;
				currentHeight = 1;
				offset = MIPMAP_OFFSET_4BPP;
			}

			while (currentWidth <= width && currentHeight <= height) {
				QImage img(currentWidth, currentHeight, QImage::Format_ARGB32);
				img.fill(Qt::transparent);
				const Twiddler twiddler(currentWidth, currentHeight);
				const int pixels = (currentWidth * currentHeight) / 2;

				if (currentWidth == 1 && currentHeight == 1) {
					img.setPixel(0, 0, palette.colorAt(data[offset] & 0xf));
					offset++;
				} else {
					for (int i=0; i<pixels; i++) {
						const QRgb pixel0 = palette.colorAt((data[offset + i] >> 0) & 0xf);
						const QRgb pixel1 = palette.colorAt((data[offset + i] >> 4) & 0xf);
						const int twidx0 = twiddler.index(i * 2 + 0);
						const int twidx1 = twiddler.index(i * 2 + 1);
						const int x0 = twidx0 % currentWidth;
						const int y0 = twidx0 / currentWidth;
						img.setPixel(x0, y0, pixel0);
						const int x1 = twidx1 % currentWidth;
						const int y1 = twidx1 / currentWidth;
						img.setPixel(x1, y1, pixel1);
					}

					offset += (currentWidth * currentHeight) / 2;
				}

				decodedImages.push_front(img);

				currentWidth *= 2;
				currentHeight *= 2;
			}

		} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP)) {
			int currentWidth = width;
			int currentHeight = height;
			int offset = 0;

			if (textureType & FLAG_MIPMAPPED) {
				currentWidth = 1;
				currentHeight = 1;
				offset = MIPMAP_OFFSET_8BPP;
			}

			while (currentWidth <= width && currentHeight <= height) {
				QImage img(currentWidth, currentHeight, QImage::Format_ARGB32);
				img.fill(Qt::transparent);
				const Twiddler twiddler(currentWidth, currentHeight);
				const int pixels = currentWidth * currentHeight;

				for (int i=0; i<pixels; i++) {
					const QRgb pixel = palette.colorAt(data[offset + i]);
					const int twidx = twiddler.index(i);
					const int x = twidx % currentWidth;
					const int y = twidx / currentWidth;
					img.setPixel(x, y, pixel);
				}

				decodedImages.push_front(img);
				offset += (currentWidth * currentHeight);

				currentWidth *= 2;
				currentHeight *= 2;
			}
		}
	} else if (is16BPP(textureType) && (textureType & FLAG_COMPRESSED)) {
		int currentWidth = width;
		int currentHeight = height;
		int offset = 2048;

		if (textureType & FLAG_MIPMAPPED) {
			currentWidth = 2;
			currentHeight = 2;
			offset += 1;
		}

		while (currentWidth <= width && currentHeight <= height) {
			QImage img(currentWidth, currentHeight, QImage::Format_ARGB32);
			QImage cui(currentWidth, currentHeight, QImage::Format_ARGB32);
			if (genPreview)
				img.fill(Qt::transparent);
			if (genCodeUsage)
				cui.fill(Qt::transparent);
			const Twiddler twiddler(currentWidth / 2, currentHeight / 2);
			const int pixels = (currentWidth / 2) * (currentHeight / 2);

			for (int i=0; i<pixels; i++) {
				const int cbidx = data[offset + i];
				const quint16 texel0 = qFromLittleEndian<quint16>(&data[cbidx * 8 + 0]);
				const quint16 texel1 = qFromLittleEndian<quint16>(&data[cbidx * 8 + 2]);
				const quint16 texel2 = qFromLittleEndian<quint16>(&data[cbidx * 8 + 4]);
				const quint16 texel3 = qFromLittleEndian<quint16>(&data[cbidx * 8 + 6]);
				const int twidx = twiddler.index(i);
				const int x = (twidx % (currentWidth / 2)) * 2;
				const int y = (twidx / (currentWidth / 2)) * 2;

				if (genPreview) {
					img.setPixel(x + 0, y + 0, to32BPP(texel0, pixelFormat));
					img.setPixel(x + 0, y + 1, to32BPP(texel1, pixelFormat));
					img.setPixel(x + 1, y + 0, to32BPP(texel2, pixelFormat));
					img.setPixel(x + 1, y + 1, to32BPP(texel3, pixelFormat));
				}

				if (genCodeUsage) {
					drawBlock(cui, x, y, 2, 2, cbidx);
				}
			}

			if (genPreview)
				decodedImages.push_front(img);
			if (genCodeUsage)
				codeUsageImages.push_front(cui);

			offset += (currentWidth * currentHeight) / 4;

			currentWidth *= 2;
			currentHeight *= 2;
		}
	} else if (isFormat(textureType, PIXELFORMAT_PAL8BPP) && (textureType & FLAG_COMPRESSED)) {
		if (paletteFilename.isEmpty())
			return false;
		Palette palette;
		if (!palette.load(paletteFilename))
			return false;

		int currentWidth = width;
		int currentHeight = height;
		int offset = 2048;

		if (textureType & FLAG_MIPMAPPED) {
			currentWidth = 4;
			currentHeight = 4;
			offset += 1;
		}

		while (currentWidth <= width && currentHeight <= height) {
			QImage img(currentWidth, currentHeight, QImage::Format_ARGB32);
			QImage cui(currentWidth, currentHeight, QImage::Format_ARGB32);
			if (genPreview)
				img.fill(Qt::transparent);
			if (genCodeUsage)
				cui.fill(Qt::transparent);
			const Twiddler twiddler(currentWidth / 4, currentHeight / 4);
			const int pixels = (currentWidth / 4) * (currentHeight / 4);

			for (int i=0; i<pixels; i++) {
				const int cbidx0 = data[offset + i * 2 + 0];
				const int cbidx1 = data[offset + i * 2 + 1];
				const int twidx = twiddler.index(i);
				const int x = (twidx % (currentWidth / 4)) * 4;
				const int y = (twidx / (currentWidth / 4)) * 4;

				if (genPreview) {
					img.setPixel(x + 0, y + 0, palette.colorAt(data[cbidx0 * 8 + 0]));
					img.setPixel(x + 0, y + 1, palette.colorAt(data[cbidx0 * 8 + 1]));
					img.setPixel(x + 1, y + 0, palette.colorAt(data[cbidx0 * 8 + 2]));
					img.setPixel(x + 1, y + 1, palette.colorAt(data[cbidx0 * 8 + 3]));
					img.setPixel(x + 0, y + 2, palette.colorAt(data[cbidx0 * 8 + 4]));
					img.setPixel(x + 0, y + 3, palette.colorAt(data[cbidx0 * 8 + 5]));
					img.setPixel(x + 1, y + 2, palette.colorAt(data[cbidx0 * 8 + 6]));
					img.setPixel(x + 1, y + 3, palette.colorAt(data[cbidx0 * 8 + 7]));
					img.setPixel(x + 2, y + 0, palette.colorAt(data[cbidx1 * 8 + 0]));
					img.setPixel(x + 2, y + 1, palette.colorAt(data[cbidx1 * 8 + 1]));
					img.setPixel(x + 3, y + 0, palette.colorAt(data[cbidx1 * 8 + 2]));
					img.setPixel(x + 3, y + 1, palette.colorAt(data[cbidx1 * 8 + 3]));
					img.setPixel(x + 2, y + 2, palette.colorAt(data[cbidx1 * 8 + 4]));
					img.setPixel(x + 2, y + 3, palette.colorAt(data[cbidx1 * 8 + 5]));
					img.setPixel(x + 3, y + 2, palette.colorAt(data[cbidx1 * 8 + 6]));
					img.setPixel(x + 3, y + 3, palette.colorAt(data[cbidx1 * 8 + 7]));
				}

				if (genCodeUsage) {
					drawBlock(cui, x + 0, y, 2, 4, cbidx0);
					drawBlock(cui, x + 2, y, 2, 4, cbidx1);
				}
			}

			if (genPreview)
				decodedImages.push_front(img);
			if (genCodeUsage)
				codeUsageImages.push_front(cui);

			offset += (currentWidth * currentHeight) / 8;

			currentWidth *= 2;
			currentHeight *= 2;
		}
	} else if (isFormat(textureType, PIXELFORMAT_PAL4BPP) && (textureType & FLAG_COMPRESSED)) {
		if (paletteFilename.isEmpty())
			return false;
		Palette palette;
		if (!palette.load(paletteFilename))
			return false;

		int currentWidth = width;
		int currentHeight = height;
		int offset = 2048;

		if (textureType & FLAG_MIPMAPPED) {
			currentWidth = 4;
			currentHeight = 4;
			offset += 1;
		}

		while (currentWidth <= width && currentHeight <= height) {
			QImage img(currentWidth, currentHeight, QImage::Format_ARGB32);
			QImage cui(currentWidth, currentHeight, QImage::Format_ARGB32);
			if (genPreview)
				img.fill(Qt::transparent);
			if (genCodeUsage)
				cui.fill(Qt::transparent);
			const Twiddler twiddler(currentWidth / 4, currentHeight / 4);

			if (textureType & FLAG_MIPMAPPED) {
				const int pixels = (currentWidth / 4) * (currentHeight / 4);

				for (int i=0; i<pixels; i++) {
					const int cbidx0 = data[offset + i - 1];
					const int cbidx1 = data[offset + i - 0];
					const int twidx = twiddler.index(i);
					const int x = (twidx % (currentWidth / 4)) * 4;
					const int y = (twidx / (currentWidth / 4)) * 4;

					if (genPreview) {
						img.setPixel(x + 0, y + 0, palette.colorAt((data[cbidx0 * 8 + 4] >> 0) & 0xf));
						img.setPixel(x + 0, y + 1, palette.colorAt((data[cbidx0 * 8 + 4] >> 4) & 0xf));
						img.setPixel(x + 1, y + 0, palette.colorAt((data[cbidx0 * 8 + 5] >> 0) & 0xf));
						img.setPixel(x + 1, y + 1, palette.colorAt((data[cbidx0 * 8 + 5] >> 4) & 0xf));
						img.setPixel(x + 0, y + 2, palette.colorAt((data[cbidx0 * 8 + 6] >> 0) & 0xf));
						img.setPixel(x + 0, y + 3, palette.colorAt((data[cbidx0 * 8 + 6] >> 4) & 0xf));
						img.setPixel(x + 1, y + 2, palette.colorAt((data[cbidx0 * 8 + 7] >> 0) & 0xf));
						img.setPixel(x + 1, y + 3, palette.colorAt((data[cbidx0 * 8 + 7] >> 4) & 0xf));
						img.setPixel(x + 2, y + 0, palette.colorAt((data[cbidx1 * 8 + 0] >> 0) & 0xf));
						img.setPixel(x + 2, y + 1, palette.colorAt((data[cbidx1 * 8 + 0] >> 4) & 0xf));
						img.setPixel(x + 3, y + 0, palette.colorAt((data[cbidx1 * 8 + 1] >> 0) & 0xf));
						img.setPixel(x + 3, y + 1, palette.colorAt((data[cbidx1 * 8 + 1] >> 4) & 0xf));
						img.setPixel(x + 2, y + 2, palette.colorAt((data[cbidx1 * 8 + 2] >> 0) & 0xf));
						img.setPixel(x + 2, y + 3, palette.colorAt((data[cbidx1 * 8 + 2] >> 4) & 0xf));
						img.setPixel(x + 3, y + 2, palette.colorAt((data[cbidx1 * 8 + 3] >> 0) & 0xf));
						img.setPixel(x + 3, y + 3, palette.colorAt((data[cbidx1 * 8 + 3] >> 4) & 0xf));
					}

					if (genCodeUsage) {
						drawBlock(cui, x + 0, y, 2, 4, cbidx0);
						drawBlock(cui, x + 2, y, 2, 4, cbidx1);
					}
				}
			} else {
				const int pixels = (currentWidth / 4) * (currentHeight / 4);

				for (int i=0; i<pixels; i++) {
					const int cbidx = data[offset + i];
					const int twidx = twiddler.index(i);
					const int x = (twidx % (currentWidth / 4)) * 4;
					const int y = (twidx / (currentWidth / 4)) * 4;

					if (genPreview) {
						img.setPixel(x + 0, y + 0, palette.colorAt((data[cbidx * 8 + 0] >> 0) & 0xf));
						img.setPixel(x + 0, y + 1, palette.colorAt((data[cbidx * 8 + 0] >> 4) & 0xf));
						img.setPixel(x + 1, y + 0, palette.colorAt((data[cbidx * 8 + 1] >> 0) & 0xf));
						img.setPixel(x + 1, y + 1, palette.colorAt((data[cbidx * 8 + 1] >> 4) & 0xf));
						img.setPixel(x + 0, y + 2, palette.colorAt((data[cbidx * 8 + 2] >> 0) & 0xf));
						img.setPixel(x + 0, y + 3, palette.colorAt((data[cbidx * 8 + 2] >> 4) & 0xf));
						img.setPixel(x + 1, y + 2, palette.colorAt((data[cbidx * 8 + 3] >> 0) & 0xf));
						img.setPixel(x + 1, y + 3, palette.colorAt((data[cbidx * 8 + 3] >> 4) & 0xf));
						img.setPixel(x + 2, y + 0, palette.colorAt((data[cbidx * 8 + 4] >> 0) & 0xf));
						img.setPixel(x + 2, y + 1, palette.colorAt((data[cbidx * 8 + 4] >> 4) & 0xf));
						img.setPixel(x + 3, y + 0, palette.colorAt((data[cbidx * 8 + 5] >> 0) & 0xf));
						img.setPixel(x + 3, y + 1, palette.colorAt((data[cbidx * 8 + 5] >> 4) & 0xf));
						img.setPixel(x + 2, y + 2, palette.colorAt((data[cbidx * 8 + 6] >> 0) & 0xf));
						img.setPixel(x + 2, y + 3, palette.colorAt((data[cbidx * 8 + 6] >> 4) & 0xf));
						img.setPixel(x + 3, y + 2, palette.colorAt((data[cbidx * 8 + 7] >> 0) & 0xf));
						img.setPixel(x + 3, y + 3, palette.colorAt((data[cbidx * 8 + 7] >> 4) & 0xf));
					}

					if (genCodeUsage) {
						drawBlock(cui, x, y, 4, 4, cbidx);
					}
				}
			}

			if (genPreview)
				decodedImages.push_front(img);
			if (genCodeUsage)
				codeUsageImages.push_front(cui);

			offset += (currentWidth * currentHeight) / 16;

			currentWidth *= 2;
			currentHeight *= 2;
		}
	}

	delete[] data;


	if (genPreview) {
		if (decodedImages.empty()) {
			qCritical() << "Failed to generate preview";
			return false;
		} else if (decodedImages.size() == 1) {
			decodedImages[0].save(previewFilename);
		} else {
			QImage img = allocatePreview(width, height, true);
			QPoint offset(0, 0);

			for (int i=0; i<decodedImages.size(); i++) {
				const QImage& tmp = decodedImages[i];
				for (int y=0; y<tmp.height(); y++)
					for (int x=0; x<tmp.width(); x++)
						img.setPixel(offset.x() + x, offset.y() + y, tmp.pixel(x, y));
				offset = nextOffset(offset, tmp.size());
			}

			img.save(previewFilename);
		}
	}

	if (genCodeUsage) {
		if (codeUsageImages.empty()) {
			qCritical() << "Failed to generate code usage";
			return false;
		} else if (codeUsageImages.size() == 1) {
			codeUsageImages[0].save(codeUsageFilename);
		} else {
			QImage img = allocatePreview(width, height, true);
			QPoint offset(0, 0);

			for (int i=0; i<codeUsageImages.size(); i++) {
				const QImage& tmp = codeUsageImages[i];
				for (int y=0; y<tmp.height(); y++)
					for (int x=0; x<tmp.width(); x++)
						img.setPixel(offset.x() + x, offset.y() + y, tmp.pixel(x, y));
				offset = nextOffset(offset, tmp.size());
			}

			img.save(codeUsageFilename);
		}
	}

	return true;
}
