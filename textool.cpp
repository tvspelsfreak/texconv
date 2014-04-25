///////////////////////////////////////////////////////////
//  ___________           ___________           .__      //
//  \__    ___/___ ___  __\__    ___/___   ____ |  |     //
//    |    |_/ __ \\  \/  / |    | /  _ \ /  _ \|  |     //
//    |    |\  ___/ >    <  |    |(  <_> |  <_> )  |__   //
//    |____| \___  >__/\_ \ |____| \____/ \____/|____/   //
//               \/      \/                              //
//                                                       //
///////////////////////////////////////////////////////////
// Converts images to the dreamcast texture format.      //
// Supports all formats that the PVR2DC supports.        //
//                                                       //
// By Anton Norgren, 2011 - 2014                         //
///////////////////////////////////////////////////////////

#include <QCommandLineParser>
#include <QStringList>
#include <QHash>
#include <QFile>
#include <QDebug>

#include <iostream>

#include "common.h"
#include "imagecontainer.h"

static bool g_verbose = false;

// Allow for colored output on unix systems
#ifndef Q_OS_WIN32
#define REDCOLOR		"\033[31m"
#define YELLOWCOLOR		"\033[33m"
#define NOCOLOR			"\033[0m"
#else
#define REDCOLOR		""
#define YELLOWCOLOR		""
#define NOCOLOR			""
#endif

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
	int a = context.line; ++a; // Make the compiler shut up about not using 'context'
	switch (type) {
	case QtDebugMsg:
		if (g_verbose)
			std::cout << qPrintable(msg) << std::endl;
		break;
	case QtWarningMsg:
		std::cerr << YELLOWCOLOR << "[WARNING] " << qPrintable(msg) << NOCOLOR << std::endl;
		break;
	case QtFatalMsg:
		std::cerr << REDCOLOR << "[FATAL] " << qPrintable(msg) << NOCOLOR << std::endl;
		break;
	case QtCriticalMsg:
		std::cerr << REDCOLOR << "[ERROR] " << qPrintable(msg) << NOCOLOR << std::endl;
		break;
	}
}

int main(int argc, char** argv) {
	qInstallMessageHandler(messageHandler);

	QHash<QString, int> supportedFormats;
	supportedFormats.insert("ARGB1555", PIXELFORMAT_ARGB1555);
	supportedFormats.insert("RGB565",	PIXELFORMAT_RGB565);
	supportedFormats.insert("ARGB4444", PIXELFORMAT_ARGB4444);
	supportedFormats.insert("YUV422",	PIXELFORMAT_YUV422);
	supportedFormats.insert("BUMPMAP",	PIXELFORMAT_BUMPMAP);
	supportedFormats.insert("PAL4BPP",	PIXELFORMAT_PAL4BPP);
	supportedFormats.insert("PAL8BPP",	PIXELFORMAT_PAL8BPP);

	QString description = "\nTexture formats:\n";
	foreach (const QString& key, supportedFormats.keys()) {
		description += "  ";
		description += key;
	}

	QCoreApplication app(argc, argv);
	QCommandLineParser parser;
	parser.addHelpOption();
	parser.setApplicationDescription(description);
	parser.addOption(QCommandLineOption(QStringList() << "i" << "in", "Input file(s). (REQUIRED)", "filename"));
	parser.addOption(QCommandLineOption(QStringList() << "o" << "out", "Output file. (REQUIRED)", "filename"));
	parser.addOption(QCommandLineOption(QStringList() << "f" << "format", "Texture format. (REQUIRED)", "format"));
	parser.addOption(QCommandLineOption(QStringList() << "m" << "mipmap", "Generate/allow mipmaps."));
	parser.addOption(QCommandLineOption(QStringList() << "c" << "compress", "Output a compressed texture."));
	parser.addOption(QCommandLineOption(QStringList() << "s" << "stride", "Output a stride texture."));
	parser.addOption(QCommandLineOption(QStringList() << "p" << "preview", "Generate a texture preview.", "filename"));
	parser.addOption(QCommandLineOption(QStringList() << "v" << "verbose", "Extra printouts."));
	parser.addOption(QCommandLineOption(QStringList() << "n" << "nearest", "Use nearest-neighbor filtering for scaling mipmaps."));
	parser.addOption(QCommandLineOption(QStringList() << "b" << "bilinear", "Use bilinear filtering for scaling mipmaps."));
	parser.addOption(QCommandLineOption("vqcodeusage", "Output an image that visualizes compression code usage.", "filename"));
	parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
	parser.process(app);

	// This is needed early for printouts
	g_verbose = parser.isSet("verbose");

	// Grab the list of input filenames
	const QStringList srcFilenames = parser.values("in");
	if (srcFilenames.isEmpty()) {
		qCritical("No input file(s) specified");
		parser.showHelp();
		return -1;
	}

	// Grab the output filename
	const QString dstFilename = parser.value("out");
	if (dstFilename.isEmpty()) {
		qCritical("No output file specified");
		parser.showHelp();
		return -1;
	}

	// Calculate palette filename
	// TODO: Make this an optional argument
	const QString palFilename = dstFilename + ".pal";

	// Grab the texture format
	const int pixelFormat = supportedFormats.value(parser.value("format"), -1);
	if (pixelFormat == -1) {
		qCritical() << "Unsupported format:" << parser.value("format");
		parser.showHelp();
		return -1;
	}

	// Now we can start building the type specifier
	int textureType = (pixelFormat << PIXELFORMAT_SHIFT);
	textureType |= parser.isSet("mipmap") ? FLAG_MIPMAPPED : 0;
	textureType |= parser.isSet("compress") ? FLAG_COMPRESSED : 0;
	textureType |= parser.isSet("stride") ? (FLAG_STRIDED | FLAG_NONTWIDDLED) : 0;

	// Determine what mode of filtering we're gonna do for mipmaps.
	// We're doing nearest-neighbor for paletted images to avoid introducing more colors.
	// It should probably be done for lossless vq textures as well, but there's no way of
	// knowing if we're gonna output one of those at this stage, so that's up to the user.
	Qt::TransformationMode mipmapFilter = isPaletted(textureType) ? Qt::FastTransformation : Qt::SmoothTransformation;
	if (parser.isSet("nearest"))	mipmapFilter = Qt::FastTransformation;
	if (parser.isSet("bilinear"))	mipmapFilter = Qt::SmoothTransformation;

	// Stride textures have a lot of restraints, and we need to check 'em all.
	if (textureType & FLAG_STRIDED) {
		if (textureType & FLAG_COMPRESSED) {
			qCritical() << "Stride textures can't be compressed.";
			return -1;
		}
		if (!(textureType & FLAG_NONTWIDDLED)) {
			qCritical() << "Stride textures can't be twiddled.";
			return -1;
		}
		if (textureType & FLAG_MIPMAPPED) {
			qCritical() << "Stride textures can't have mipmaps.";
			return -1;
		}
		if (isPaletted(textureType) || isFormat(textureType, PIXELFORMAT_BUMPMAP)) {
			qCritical() << "Only RGB565, ARGB1555, ARGB4444 and YUV422 can be strided.";
			return -1;
		}
	}

	// Time to load the image(s)
	ImageContainer images;
	if (!images.load(srcFilenames, textureType, mipmapFilter)) {
		return -1;
	}

	if (textureType & FLAG_STRIDED) {
		// Now that the image is loaded and its width is known we can put
		// the stride setting in the texture type field (bits 0-4).
		const int strideSetting = (images.width() / 32);
		textureType |= strideSetting;
	}



	QFile out(dstFilename);
	if (!out.open(QIODevice::WriteOnly)) {
		qCritical() << "Failed to open" << dstFilename;
		return false;
	}
	QDataStream stream(&out);
	stream.setByteOrder(QDataStream::LittleEndian);

	// Write texture header
	const int expectedSize = writeTextureHeader(stream, images.width(), images.height(), textureType);
	const int positionBeforeData = (int)stream.device()->pos();

	// Write texture data
	if (isPaletted(textureType)) {
		convertPaletted(stream, images, textureType, palFilename);
	} else {
		convert16BPP(stream, images, textureType);
	}

	// Pad the texture data block to 32 bytes
	const int positionAfterData = (int)stream.device()->pos();
	const int padding = expectedSize - (positionAfterData - positionBeforeData);
	if (padding > 0) {
		if (padding >= 32)
			qWarning() << "Padding is" << padding << "but it should be less than 32!";
		writeZeroes(stream, padding);
		qDebug() << "Added" << padding << "bytes of padding";
	}

	out.close();
	qDebug() << "Saved texture" << dstFilename;



	// Generate preview and/or vq code usage images
	const QString previewFilename = parser.value("preview");
	const QString codeUsageFilename = (textureType & FLAG_COMPRESSED) ? parser.value("vqcodeusage") : "";
	if (!previewFilename.isEmpty() || !codeUsageFilename.isEmpty()) {
		if (generatePreview(dstFilename, palFilename, previewFilename, codeUsageFilename)) {
			if (!previewFilename.isEmpty())		qDebug() << "Saved preview image" << previewFilename;
			if (!codeUsageFilename.isEmpty())	qDebug() << "Saved code usage image" << codeUsageFilename;
		} else {
			if (!previewFilename.isEmpty())		qDebug() << "Failed to save" << previewFilename;
			if (!codeUsageFilename.isEmpty())	qDebug() << "Failed to save" << codeUsageFilename;
		}
	}

    return 0;
}
