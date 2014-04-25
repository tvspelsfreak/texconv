#include <QDebug>
#include "imagecontainer.h"
#include "common.h"

bool ImageContainer::load(const QStringList& filenames, const int textureType, const Qt::TransformationMode mipmapFilter) {
	const bool compressed	= (textureType & FLAG_COMPRESSED);
	const bool mipmapped	= (textureType & FLAG_MIPMAPPED);

	if ((filenames.size() > 1) && !mipmapped) {
		qCritical() << "Only one input file may be specified if no mipmap flag has been given.";
		return false;
	}

	// Load all given images
	foreach (const QString& filename, filenames) {
		const QImage img(filename);

		if (img.isNull()) {
			qCritical() << "Failed to load image" << filename;
			return false;
		}

		if (!isValidSize(img.width(), img.height(), textureType)) {
			qCritical("Image %s has an invalid texture size %dx%d", qPrintable(filename), img.width(), img.height());
			return false;
		}

		if ((compressed || mipmapped) && (img.width() != img.height())) {
			qCritical() << "Image" << filename << "is not square. Input images for compressed and mipmapped textures must be square";
			return false;
		}

		textureSize = textureSize.expandedTo(img.size());
		images.insert(img.width(), img);

		qDebug() << "Loaded image" << filename;
	}

	if (mipmapped) {
		if (mipmapFilter == Qt::FastTransformation) {
			qDebug("Using nearest-neighbor filtering for mipmaps");
		} else if (mipmapFilter == Qt::SmoothTransformation) {
			qDebug("Using bilinear filtering for mipmaps");
		}

		// Generate any missing images by scaling down the size above them
		for (int size=(TEXTURE_SIZE_MAX/2); size>=1; size/=2) {
			if (images.contains(size*2) && !images.contains(size)) {
				const QImage mipmap = images.value(size*2).scaledToWidth(size, mipmapFilter);
				images.insert(size, mipmap);
				qDebug("Generated %dx%d mipmap", size, size);
			}
		}
	}

	// Make sure we have at least one ok image
	if (width() < TEXTURE_SIZE_MIN || height() < TEXTURE_SIZE_MIN) {
		qCritical("At least one input image must be 8x8 or larger.");
		return false;
	}

	// Save keys for easy iteration
	keys = images.keys();
	std::sort(keys.begin(), keys.end());

	return true;
}

void ImageContainer::unloadAll() {
	textureSize = QSize(0, 0);
	images.clear();
	keys.clear();
}

QImage ImageContainer::getByIndex(int index, bool ascending) const {
	if (index >= keys.size()) {
		return QImage();
	} else {
		index = ascending ? index : (keys.size() - index - 1);
		int size = keys[index];
		return images.value(size);
	}
}


