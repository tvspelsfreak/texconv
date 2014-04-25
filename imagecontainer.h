#ifndef IMAGECONTAINER_H
#define IMAGECONTAINER_H

#include <QStringList>
#include <QSize>
#include <QMap>
#include <QList>
#include <QImage>
#include "common.h"

// Wrapper class for a collection of QImages.
// Allows for easy access by size and iteration from smallest to largest or
// largest to smallest texture.
//
// The container has two different states:
// If HasMipmaps() == false:
//   There's one image in the container, and it may be rectangular.
// If HasMipmaps() == true:
//   The container has a number of square images ranging from the largest one
//   loaded down to 1x1 pixels.
class ImageContainer {
public:

	/**
	 * Loads all images given in filenames. If an image of the same size is loaded twice,
	 * the previous instance will be overwritten.
	 *
	 * If flags does not contain FLAG_MIPMAPPED:
	 *   Only one filename may be given and the image may be rectangular.
	 * If flags conatain FLAG_MIPMAPPED:
	 *   Any number of filenames may be given. All images must be square. Any missing
	 *   mipmap levels will be generated automatically.
	 */
	bool load(const QStringList& filenames, const int textureType, const Qt::TransformationMode mipmapFilter);
	void unloadAll();

	bool hasMipmaps() const { return images.size() > 1; }
	bool hasSize(const int size) const { return images.contains(size); }

	QImage getByIndex(int index, bool ascending = true) const;
	QImage getBySize(const int size) const { return images.value(size); }

	int imageCount() const { return images.size(); }
	int width() const { return textureSize.width(); }
	int height() const { return textureSize.height(); }
	QSize size() const { return textureSize; }


private:
	QSize				textureSize = QSize(0, 0);
	QMap<int, QImage>	images;
	QList<int>			keys;
};

#endif // IMAGECONTAINER_H
