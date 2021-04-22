/*
Decoder for lossy image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"
#include "hilbert.h"

void doit(float *tree, float *output, int stride, int level, int depth, int quant)
{
	int length = 1 << level;
	int pixels = length * length;
	if (level == depth) {
		for (int i = 0; i < pixels; ++i)
			output[i*stride] = tree[i];
		return;
	}
	if (level == 0)
		tree[0] /= quant << depth;
	for (int i = 0; i < 4 * pixels; ++i)
		tree[pixels+i] /= quant << (depth - level - 1);
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			float avg = tree[length*j+i];
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					tree[pixels+length*2*(j*2+y)+i*2+x] += avg;
		}
	}
	doit(tree+pixels, output, stride, level+1, depth, quant);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s input.lqt output.ppm\n", argv[0]);
		return 1;
	}
	struct bits *bits = bits_reader(argv[1]);
	if (!bits)
		return 1;
	int mode = get_bit(bits);
	int depth = get_vli(bits);
	int length = 1 << depth;
	int pixels = length * length;
	int quant[3];
	for (int i = 0; i < 3; ++i)
		quant[i] = get_vli(bits);
	int tree_size = (pixels * 4 - 1) / 3;
	float *tree = malloc(sizeof(float) * tree_size);
	struct image *output = new_image(argv[2], length, length);
	for (int j = 0; j < 3; ++j) {
		float *level = tree;
		for (int d = 0; d <= depth; ++d) {
			int len = 1 << d;
			int size = len * len;
			for (int i = 0; i < size; ++i) {
				float val = get_vli(bits);
				if (val) {
					if (get_bit(bits))
						val = -val;
				} else {
					int cnt = get_vli(bits);
					for (int k = 0; k < cnt; ++k)
						level[hilbert(len, i++)] = 0;
				}
				level[hilbert(len, i)] = val;
			}
			level += size;
		}
		doit(tree, output->buffer+j, 3, 0, depth, quant[j]);
	}
	close_reader(bits);
	if (mode)
		rgb_image(output);
	if (!write_ppm(output))
		return 1;
	return 0;
}

