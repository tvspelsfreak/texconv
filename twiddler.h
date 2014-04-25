#ifndef TWIDDLER_H
#define TWIDDLER_H

/**
 * Class to allow for easy twiddling of textures
 */
class Twiddler {
public:

	Twiddler(int w, int h);
	~Twiddler();

	int	index(int x, int y) const { return m_index[y * m_width + x]; }
	int index(int i)		const { return m_index[i]; }

private:

	int twiddle(int* output, int stride, int x, int y, int blocksize, int seq) const;

	int		m_width;
	int		m_height;
	int*	m_index;
};

#endif // TWIDDLER_H
