#ifndef PALETTE_H
#define PALETTE_H

#include <QtGlobal>
#include <QHash>
#include <QColor>

class ImageContainer;

class Palette {
public:

	Palette() {}
	Palette(const ImageContainer& images);

	int colorCount() const { return colors.size(); }

	void clear() { colors.clear(); }

	void insert(const QRgb color);

	int indexOf(const QRgb color) const { return colors.value(color, 0); }
	QRgb colorAt(const int index) const { return colors.key(index, qRgb(0, 0, 0)); }

	bool load(const QString& filename);
	bool save(const QString& filename) const;

private:
	// "Color" <=> "Palette index"
	QHash<QRgb, int> colors;
};

#endif // PALETTE_H
