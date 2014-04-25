#include "palette.h"
#include "imagecontainer.h"

#include <QFile>
#include <QDebug>

Palette::Palette(const ImageContainer& images) {
	for (int i=0; i<images.imageCount(); i++) {
		const QImage& img = images.getByIndex(i);
		for (int y=0; y<img.height(); y++)
			for (int x=0; x<img.width(); x++)
				insert(img.pixel(x, y));
	}
}

void Palette::insert(const QRgb color) {
	if (!colors.contains(color))
		colors.insert(color, colors.size());
}

bool Palette::save(const QString& filename) const {
	QFile file(filename);

	if (file.open(QIODevice::WriteOnly)) {
		QDataStream out(&file);
		out.setByteOrder(QDataStream::LittleEndian);

		// Write header
		out.writeRawData(PALETTE_MAGIC, 4);
		out << (qint32)colors.size();

		// Write the colors
		for (int i=0; i<colors.size(); i++)
			out << (quint32)colors.key(i);

		file.close();
		return true;
	}

	qCritical() << "Failed to open" << filename;
	return false;
}

bool Palette::load(const QString& filename) {
	QFile file(filename);

	if (file.open(QIODevice::ReadOnly)) {
		char magic[4];
		qint32 numColors = 0;

		QDataStream in(&file);
		in.setByteOrder(QDataStream::LittleEndian);

		// Read header
		in.readRawData(magic, 4);
		if (memcmp(magic, PALETTE_MAGIC, 4) != 0) {
			qCritical() << filename << "is not a valid palette file";
			return false;
		}
		in >> numColors;

		// Read colors
		colors.clear();
		for (int i=0; i<numColors; i++) {
			quint32 color = 0xFF000000;
			in >> color;
			colors.insert((QRgb)color, i);
		}

		file.close();
		return true;
	}

	qCritical() << "Failed to open" << filename;
	return false;
}
