/*
Decoder for lossless image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"
#include "hilbert.h"

void doit(int *tree, int *output, int level, int depth)
{
	int length = 1 << level;
	int pixels = length * length;
	if (level == depth) {
		for (int i = 0; i < pixels; ++i)
			output[i] = tree[i];
		return;
	}
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			int avg = tree[length*j+i];
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					tree[pixels+length*2*(j*2+y)+i*2+x] += avg;
		}
	}
	doit(tree+pixels, output, level+1, depth);
}

void copy(int *output, int *input, int width, int height, int length, int stride)
{
	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			output[(width*j+i)*stride] = input[length*j+i];
}

int decode(struct bits_reader *bits, int *level, int len, int plane)
{
	int ret = get_vli(bits);
	if (ret < 0)
		return ret;
	int size = len * len;
	for (int i = ret; i < size; i += ret + 1) {
		level[hilbert(len, i)] |= 1 << plane;
		if ((ret = get_vli(bits)) < 0)
			return ret;
	}
	return 0;
}

void finalize(int *val, int num, int planes)
{
	int mask = 1 << (planes-1);
	for (int i = 0; i < num; ++i)
		if (val[i] & mask)
			val[i] = -(val[i]^mask);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s input.lqt output.ppm\n", argv[0]);
		return 1;
	}
	struct bits_reader *bits = bits_reader(argv[1]);
	if (!bits)
		return 1;
	int mode = get_bit(bits);
	int width = get_vli(bits);
	int height = get_vli(bits);
	if ((mode|width|height) < 0)
		return 1;
	int length = 1;
	int depth = 0;
	while (length < width || length < height)
		length = 1 << ++depth;
	int pixels = length * length;
	int tree_size = (pixels * 4 - 1) / 3;
	int *tree = malloc(sizeof(int) * 3 * tree_size);
	for (int i = 0; i < 3 * tree_size; ++i)
		tree[i] = 0;
	int planes = 8 + mode;
	for (int plane = planes-1; plane >= 0; --plane)
		for (int d = 0, len = 1, *level = tree; d <= depth; ++d, level += len*len, len *= 2)
			for (int chan = 0; chan < 3; ++chan)
				if (decode(bits, level+chan*tree_size, len, plane))
					goto end;
end:
	finalize(tree, 3 * tree_size, planes);
	int *output = malloc(sizeof(int) * pixels);
	struct image *image = new_image(argv[2], width, height);
	for (int chan = 0; chan < 3; ++chan) {
		doit(tree+chan*tree_size, output, 0, depth);
		copy(image->buffer+chan, output, width, height, length, 3);
	}
	free(tree);
	free(output);
	close_reader(bits);
	if (mode)
		rgb_image(image);
	if (!write_ppm(image))
		return 1;
	delete_image(image);
	return 0;
}

