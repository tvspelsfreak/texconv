#ifndef VQTOOLS_H
#define VQTOOLS_H

#include <QVector>
#include <QString>
#include <QDebug>
#include <QElapsedTimer>
#include <QColor>
#include <QFile>
#include <cmath>

// N-dimensional vectors, for input to a VectorQuantizer.
template <uint N>
class Vec {
public:
	Vec(uint hval = 0) : hashVal(hval) {}
	Vec(const Vec<N>& other);
	void	zero();
	void	operator= (const Vec<N>& other);
	bool	operator== (const Vec<N>& other) const;
	void	operator+= (const Vec<N>& other);
	void	operator-= (const Vec<N>& other);
	Vec<N>	operator+ (const Vec<N>& other) const;
	Vec<N>	operator- (const Vec<N>& other) const;
	void	addMultiplied(const Vec<N>& other, float x);
	void	operator/= (float x);
	float	operator[] (int index) const;
	void	set(int index, float value);
	float	lengthSquared() const;
	float	length() const;
	void	setLength(float len);
	void	normalize();
	void	print() const;
	static float distanceSquared(const Vec<N>& a, const Vec<N>& b);
	uint	hash() const;
	void	setHash(uint h) { hashVal = h; }
private:
	float	v[N];
	uint	hashVal; // Only used for the constant input vectors, so we only need to calc once.
	quint64	lololol; // Speeds up the average compression by a couple of seconds on my machine. Probably some alignment stuff.
};

// VectorQuantizer, compresses N-dimensional vectors
template <uint N>
class VectorQuantizer {
public:
	void clear() { codes.clear(); }
	int	codeCount() const { return codes.size(); }
	int findClosest(const Vec<N>& vec) const;
	const Vec<N>& codeVector(int index) const { return codes[index].codeVec; }
	void compress(const QVector<Vec<N>>& vectors, int numCodes);
	bool writeReportToFile(const QString& filename);
private:
	int findBestSplitCandidate() const;
	void removeUnusedCodes();
	void place(const QHash<Vec<N>, int>& vectors);
	void split();
	void splitCode(int index);

	struct Code {
		int		vecCount;
		Vec<N>	vecSum;
		float	maxDistance;
		Vec<N>	maxDistanceVec;
		Vec<N>	codeVec;
	};
	QVector<Code> codes;
};

template<uint N>
void rgb2vec(const QRgb& rgb, Vec<N>& vec, uint offset = 0) {
	const QColor color(rgb);
	vec.set(offset + 0, color.redF());
	vec.set(offset + 1, color.greenF());
	vec.set(offset + 2, color.blueF());
}

template<uint N>
void argb2vec(const QRgb& argb, Vec<N>& vec, uint offset = 0) {
	const QColor color = QColor::fromRgba(argb);
	vec.set(offset + 0, color.alphaF());
	vec.set(offset + 1, color.redF());
	vec.set(offset + 2, color.greenF());
	vec.set(offset + 3, color.blueF());
}

template<uint N>
void vec2rgb(const Vec<N>& vec, QRgb& rgb, uint offset = 0) {
	const QColor color = QColor::fromRgbF(vec[offset + 0], vec[offset + 1], vec[offset + 2]);
	rgb = color.rgb();
}

template<uint N>
void vec2argb(const Vec<N>& vec, QRgb& argb, uint offset = 0) {
	const QColor color = QColor::fromRgbF(vec[offset + 1], vec[offset + 2], vec[offset + 3], vec[offset + 0]);
	argb = color.rgba();
}

//////////////////////////////////////////////////////
// Implementations below, nothing to see here...
//////////////////////////////////////////////////////

template<uint N>
inline Vec<N>::Vec(const Vec<N> &other) {
	*this = other;
}

template<uint N>
inline void Vec<N>::zero() {
	for (uint i=0; i<N; ++i)
		v[i] = 0;
}

template<uint N>
inline void Vec<N>::operator= (const Vec<N>& other) {
	for (uint i=0; i<N; ++i)
		v[i] = other.v[i];
	hashVal = other.hashVal;
}

template<uint N>
inline bool Vec<N>::operator== (const Vec<N>& other) const {
	for (uint i=0; i<N; ++i)
		if (qAbs(v[i] - other.v[i]) > 0.001f)
			return false;
	return true;
}

template<uint N>
inline Vec<N> Vec<N>::operator+ (const Vec<N>& other) const {
	Vec<N> ret;
	for (uint i=0; i<N; ++i)
		ret.v[i] = v[i] + other.v[i];
	return ret;
}

template<uint N>
inline Vec<N> Vec<N>::operator- (const Vec<N>& other) const {
	Vec<N> ret;
	for (uint i=0; i<N; ++i)
		ret.v[i] = v[i] - other.v[i];
	return ret;
}

template<uint N>
inline void Vec<N>::addMultiplied(const Vec<N>& other, float x) {
	for (uint i=0; i<N; ++i)
		v[i] += (other.v[i] * x);
}

template<uint N>
inline void Vec<N>::operator+= (const Vec<N>& other) {
	for (uint i=0; i<N; ++i)
		v[i] += other.v[i];
}

template<uint N>
inline void Vec<N>::operator-= (const Vec<N>& other) {
	for (uint i=0; i<N; ++i)
		v[i] -= other.v[i];
}

template<uint N>
inline void Vec<N>::operator/= (float x) {
	const float invx = 1.0f / x;
	for (uint i=0; i<N; ++i)
		v[i] *= invx;
}

template<uint N>
inline float Vec<N>::operator[] (int index) const {
	return v[index];
}

template<uint N>
inline void Vec<N>::set(int index, float value) {
	v[index] = value;
}

template<uint N>
inline float Vec<N>::length() const {
	return sqrt(lengthSquared());
}

template<uint N>
inline float Vec<N>::lengthSquared() const {
	float ret = 0;
	for (uint i=0; i<N; ++i)
		ret += (v[i] * v[i]);
	return ret;
}

template<uint N>
inline void Vec<N>::setLength(float len) {
	float x = (1.0f / length()) * len;
	for (uint i=0; i<N; ++i)
		v[i] *= x;
}

template<uint N>
inline void Vec<N>::normalize() {
	const float invlen = 1.0f / length();
	for (uint i=0; i<N; ++i)
		v[i] *= invlen;
}

template<uint N>
void Vec<N>::print() const {
	QString str = "{ ";
	for (uint i=0; i<N; ++i) {
		str.append(QString::number(v[i], 'f'));
		str.append(' ');
	}
	str.append('}');
	qDebug() << str;
}

template<uint N>
float Vec<N>::distanceSquared(const Vec<N>& a, const Vec<N>& b) {
	return (a - b).lengthSquared();
}

template<uint N>
inline uint	Vec<N>::hash() const {
	return hashVal;
}

template<uint N>
uint qHash(const Vec<N>& vec) {
	return vec.hash();
}

template<uint N>
int VectorQuantizer<N>::findClosest(const Vec<N> &vec) const {
	// TODO: This search is O(n), and the place where most of the
	// compression time is spent. Find a better algorithm.
	//
	// kd-trees are not an option, they won't perform better than
	// linear searches at high dimensions unless you have a lot of
	// vectors. Specifically nVectors > 2^DIM.
	//
	// Locality sensitive hashing should speed it up, but comes
	// at a quality cost.

	if (codes.size() <= 1)
		return 0;
	int closestIndex = 0;
	float closestDistance = Vec<N>::distanceSquared(codes[0].codeVec, vec);

	for (int i=1; i<codes.size(); i++) {
		float distance = Vec<N>::distanceSquared(codes[i].codeVec, vec);
		if (distance < closestDistance)	{
			closestIndex = i;
			closestDistance = distance;
			if (closestDistance < 0.0001f)
				return closestIndex;
		}
	}
	return closestIndex;
}

template<uint N>
int VectorQuantizer<N>::findBestSplitCandidate() const {
	int retval = -1;
	float furthest = 0;
	for (int i=0; i<codes.size(); i++) {
		if (codes[i].vecCount > 1 && codes[i].maxDistance > furthest) {
			furthest = codes[i].maxDistance;
			retval = i;
		}
	}
	return retval;
}

template<uint N>
void VectorQuantizer<N>::removeUnusedCodes() {
	int removed = 0;
	for (int i=0; i<codes.size(); i++) {
		if (codes[i].vecCount == 0) {
			codes.removeAt(i);
			--i;
			++removed;
		}
	}
	if (removed > 0)
		qDebug() << "Removed" << removed << "unused codes";
}

template<uint N>
void VectorQuantizer<N>::place(const QHash<Vec<N>, int> &vecs) {
	// Reset the encoding-related code variables
	for (int i=0; i<codes.size(); i++) {
		codes[i].vecCount = 0;
		codes[i].vecSum.zero();
		codes[i].maxDistance = 0;
		codes[i].maxDistanceVec.zero();
	}

	QHashIterator<Vec<N>, int> it(vecs);
	while (it.hasNext()) {
		it.next();
		const Vec<N>& vec = it.key();
		const int count = it.value();

		// Find closest code
		Code& code = codes[findClosest(vec)];

		// Update the average
		code.vecSum.addMultiplied(vec, count);
		code.vecCount += count;

		// Update the max distance if needed
		float distance = Vec<N>::distanceSquared(code.codeVec, vec);
		if (distance > code.maxDistance) {
			code.maxDistance = distance;
			code.maxDistanceVec = vec;
		}
	}

	for (int i=0; i<codes.size(); i++) {
		if (codes[i].vecCount > 0) {
			// Normalize the sum and update the code vector
			codes[i].vecSum /= (float)codes[i].vecCount;
			codes[i].codeVec = codes[i].vecSum;
		}
	}
}

template<uint N>
void VectorQuantizer<N>::split() {
	// The size will change and we don't wanna iterate
	// over the new codes.
	const int SIZE = codes.size();
	for (int i=0; i<SIZE; i++)	{
		if (codes[i].vecCount > 1) {
			splitCode(i);
		}
	}
}

template<uint N>
void VectorQuantizer<N>::splitCode(int index) {
	// Split this code into two by moving the code vector away from the max
	// distance vector and the new code vector towards the max distance vector
	// byt a tiny amount and let the place() iterations tear them apart.
	Code& code = codes[index];
	Vec<N> diff = code.maxDistanceVec - code.codeVec;
	diff.setLength(0.01);
	Vec<N> newVec = code.codeVec;
	newVec += diff;
	code.codeVec -= diff;
	codes.push_back(Code());
	codes.last().codeVec = newVec;
}

template<uint N>
void VectorQuantizer<N>::compress(const QVector<Vec<N>>& vectors, int numCodes) {
	int splits = 0;
	int repairs = 0;

	QElapsedTimer timer;
	timer.start();

	// The input vectors don't have to be in a specific order, so to save a lot
	// of time later, we remove all duplicates and store the vectors in a hash
	// <vec, num_occurances>. This isn't as slow as it sounds since the vectors
	// have very efficient hashing.
	QHash<Vec<N>, int> rle;
	for (int i=0; i<vectors.size(); i++) {
		const Vec<N>& vec = vectors[i];
		if (rle.contains(vec))
			rle[vec]++;
		else
			rle.insert(vec, 1);
	}

	qDebug() << "RLE completed in" << timer.elapsed() << "ms";
	qDebug() << "RLE result:" << vectors.size() << "=>" << rle.size();

	// Start out with 1 code.
	codes.clear();
	codes.resize(1);
	codes.reserve(numCodes);

	// Place the average of all vectors in that first code.
	place(rle);

	// Split the codebook as many times as we can.
	while ((codes.size() * 2) <= numCodes) {
		int codesBefore = codes.size();

		split();
		place(rle);
		place(rle);
		place(rle);
		removeUnusedCodes();

		if (codes.size() == codesBefore) {
			qDebug() << "Could not further improve the codebook by splitting";
			break;
		}

		splits++;
		qDebug() << "Split" << splits << "done. Codes:" << codeCount();
	}

	// Fill in the rest of the codes by splitting the one with the highest error
	// until we have all the codes we want, or can't split anymore.
	while (codes.size() < numCodes) {
		const int codesBefore = codes.size();
		const int n = numCodes - codesBefore;

		for (int i=0; i<n; i++) {
			const int splitCandidate = findBestSplitCandidate();
			if (splitCandidate == -1)
				break;

			splitCode(splitCandidate);

			// Reset this so it won't be found in the next iteration
			codes[splitCandidate].maxDistance = 0;
		}

		if (codes.size() == codesBefore) {
			qDebug() << "Could not further improve the codebook by repairing";
			break;
		}

		place(rle);
		place(rle);
		place(rle);
		removeUnusedCodes();

		if (codes.size() == codesBefore) {
			qDebug() << "Could not further improve the codebook by repairing";
			break;
		}

		repairs++;
		qDebug() << "Repair" << repairs << "done. Codes:" << codeCount();
	}

	qDebug() << "Compression completed in" << timer.elapsed() << "ms";
}

template<uint N>
bool VectorQuantizer<N>::writeReportToFile(const QString& filename) {
		QFile file(filename);
		if (!file.open(QIODevice::WriteOnly)) {
			qCritical() << "Failed to open" << filename;
			return false;
		}
		QTextStream stream(&file);

		for (int i=0; i<codes.size(); i++) {
			stream << "Code: " << i << "\tUses: " << codes[i].vecCount << "\tError: " << codes[i].maxDistance << "\n";
		}

		file.close();
		return true;
}

#endif // VQTOOLS_H
