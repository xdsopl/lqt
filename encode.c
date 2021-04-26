/*
Encoder for lossy image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"
#include "hilbert.h"

void doit(float *tree, float *input, int stride, int level, int depth, int quant)
{
	int length = 1 << level;
	int pixels = length * length;
	if (level == depth) {
		for (int i = 0; i < pixels; ++i)
			tree[i] = input[i*stride];
		return;
	}
	doit(tree+pixels, input, stride, level+1, depth, quant);
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			float sum = 0.f;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					sum += tree[pixels+length*2*(j*2+y)+i*2+x];
			float avg = sum / 4.f;
			tree[length*j+i] = avg;
			avg = nearbyintf((quant<<depth) * avg) / (quant<<depth);
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					tree[pixels+length*2*(j*2+y)+i*2+x] -= avg;
		}
	}
	if (level == 0)
		tree[0] = nearbyintf((quant<<depth) * tree[0]);
	quant <<= depth - level - 1;
	for (int i = 0; i < 4 * pixels; ++i)
		tree[pixels+i] = nearbyintf(quant * tree[pixels+i]);
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
	if (argc != 3 && argc != 6 && argc != 7) {
		fprintf(stderr, "usage: %s input.ppm output.lqt [Q0 Q1 Q2] [MODE]\n", argv[0]);
		return 1;
	}
	struct image *input = read_ppm(argv[1]);
	if (!input || input->width != input->height || !pow2(input->width))
		return 1;
	int mode = 1;
	if (argc == 7)
		mode = atoi(argv[6]);
	int length = input->width;
	int pixels = length * length;
	int depth = ilog2(length);
	int quant[3] = { 128, 32, 32 };
	if (argc >= 6)
		for (int i = 0; i < 3; ++i)
			quant[i] = atoi(argv[3+i]);
	int tree_size = (pixels * 4 - 1) / 3;
	float *tree = malloc(sizeof(float) * tree_size);
	if (mode)
		ycbcr_image(input);
	struct bits *bits = bits_writer(argv[2]);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_vli(bits, depth);
	for (int i = 0; i < 3; ++i)
		put_vli(bits, quant[i]);
	int zeros = 0;
	for (int j = 0; j < 3; ++j) {
		if (!quant[j])
			continue;
		doit(tree, input->buffer+j, 3, 0, depth, quant[j]);
		float *level = tree;
		for (int d = 0; d <= depth; ++d) {
			int len = 1 << d;
			int size = len * len;
			for (int i = 0; i < size; ++i) {
				if (level[hilbert(len, i)]) {
					put_vli(bits, fabsf(level[hilbert(len, i)]));
					put_bit(bits, level[hilbert(len, i)] < 0.f);
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

