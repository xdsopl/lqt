/*
Encoder for lossless image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"
#include "hilbert.h"

void doit(int *tree, int *input, int level, int depth)
{
	int length = 1 << level;
	int pixels = length * length;
	if (level == depth) {
		for (int i = 0; i < pixels; ++i)
			tree[i] = input[i];
		return;
	}
	doit(tree+pixels, input, level+1, depth);
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			int sum = 0;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					sum += tree[pixels+length*2*(j*2+y)+i*2+x];
			if (sum < 0)
				sum -= 2;
			else
				sum += 2;
			int avg = sum / 4;
			tree[length*j+i] = avg;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					tree[pixels+length*2*(j*2+y)+i*2+x] -= avg;
		}
	}
}

void copy(int *output, int *input, int width, int height, int length, int stride)
{
	for (int j = 0; j < length; ++j)
		for (int i = 0; i < length; ++i)
			if (j < height && i < width)
				output[length*j+i] = input[(width*j+i)*stride];
			else
				output[length*j+i] = 0;
}

void encode(struct bits_writer *bits, int *level, int len)
{
	int size = len * len;
	for (int i = 0; i < size; ++i) {
		if (level[hilbert(len, i)]) {
			put_vli(bits, abs(level[hilbert(len, i)]));
			put_bit(bits, level[hilbert(len, i)] < 0);
		} else {
			put_vli(bits, 0);
			int k = i + 1;
			while (k < size && !level[hilbert(len, k)])
				++k;
			--k;
			put_vli(bits, k - i);
			i = k;
		}
	}
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
	struct image *image = read_ppm(argv[1]);
	if (!image)
		return 1;
	if (mode)
		rct_image(image);
	int width = image->width;
	int height = image->height;
	int length = 1;
	int depth = 0;
	while (length < width || length < height)
		length = 1 << ++depth;
	int pixels = length * length;
	int *input = malloc(sizeof(int) * pixels);
	int tree_size = (pixels * 4 - 1) / 3;
	int *tree = malloc(sizeof(int) * 3 * tree_size);
	for (int chan = 0; chan < 3; ++chan) {
		copy(input, image->buffer+chan, width, height, length, 3);
		doit(tree+chan*tree_size, input, 0, depth);
	}
	free(input);
	delete_image(image);
	struct bits_writer *bits = bits_writer(argv[2], 1 << 24);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_vli(bits, width);
	put_vli(bits, height);
	bits_flush(bits);
	for (int d = 0, len = 1, *level = tree; d <= depth; ++d, level += len*len, len *= 2) {
		for (int chan = 0; chan < 3; ++chan) {
			encode(bits, level+chan*tree_size, len);
			bits_flush(bits);
		}
	}
	free(tree);
	int cnt = bits_count(bits);
	int bytes = (cnt + 7) / 8;
	int kib = (bytes + 512) / 1024;
	fprintf(stderr, "%d bits (%d KiB) encoded\n", cnt, kib);
	close_writer(bits);
	return 0;
}

