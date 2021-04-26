/*
Encoder for lossless image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"
#include "hilbert.h"

void doit(int *tree, int *input, int stride, int level, int depth)
{
	int length = 1 << level;
	int pixels = length * length;
	if (level == depth) {
		for (int i = 0; i < pixels; ++i)
			tree[i] = input[i*stride];
		return;
	}
	doit(tree+pixels, input, stride, level+1, depth);
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			int sum = 0;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					sum += tree[pixels+length*2*(j*2+y)+i*2+x];
			int avg = (sum + 2) / 4;
			tree[length*j+i] = avg;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					tree[pixels+length*2*(j*2+y)+i*2+x] -= avg;
		}
	}
}

int pow2(int N)
{
	return !(N & (N - 1));
}

int ilog2(int x)
{
	int l = -1;
	for (; x > 0; x /= 2)
		++l;
	return l;
}

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: %s input.ppm output.lqt [MODE]\n", argv[0]);
		return 1;
	}
	int mode = 1;
	if (argc == 4)
		mode = atoi(argv[3]);
	struct image *input = read_ppm(argv[1]);
	if (!input || input->width != input->height || !pow2(input->width))
		return 1;
	if (mode)
		rct_image(input);
	int length = input->width;
	int pixels = length * length;
	int depth = ilog2(length);
	int tree_size = (pixels * 4 - 1) / 3;
	int *tree = malloc(sizeof(int) * tree_size);
	struct bits *bits = bits_writer(argv[2]);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_vli(bits, depth);
	int zeros = 0;
	for (int j = 0; j < 3; ++j) {
		doit(tree, input->buffer+j, 3, 0, depth);
		int *level = tree;
		for (int d = 0; d <= depth; ++d) {
			int len = 1 << d;
			int size = len * len;
			for (int i = 0; i < size; ++i) {
				if (level[hilbert(len, i)]) {
					put_vli(bits, abs(level[hilbert(len, i)]));
					put_bit(bits, level[hilbert(len, i)] < 0);
				} else {
					int pos0 = ftell(bits->file) * 8 + bits->cnt;
					put_vli(bits, 0);
					int k = i + 1;
					while (k < size && !level[hilbert(len, k)])
						++k;
					--k;
					put_vli(bits, k - i);
					i = k;
					int pos1 = ftell(bits->file) * 8 + bits->cnt;
					zeros += pos1 - pos0;
				}
			}
			level += size;
		}
	}
	fprintf(stderr, "bits used to encode zeros: %d%%\n", (100 * zeros) / (int)(ftell(bits->file) * 8 + bits->cnt));
	close_writer(bits);
	return 0;
}

