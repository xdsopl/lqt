/*
Decoder for lossy image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"

void doit(float *output, float *input, int stride, int level, int depth, int quant)
{
	int length = 1 << level;
	int pixels = length * length;
	if (level == depth) {
		for (int i = 0; i < pixels; ++i)
			output[i*stride] = input[i*stride];
		return;
	}
	if (level == 0)
		input[0] /= quant << depth;
	for (int i = 0; i < 4 * pixels; ++i)
		input[(pixels+i)*stride] /= quant << (depth - level - 1);
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			float avg = input[(length*j+i)*stride];
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					input[(pixels+length*2*(j*2+y)+i*2+x)*stride] += avg;
		}
	}
	doit(output, input+pixels*stride, stride, level+1, depth, quant);
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
	float *input = malloc(sizeof(float) * 3 * tree_size);
	for (int j = 0; j < 3; ++j) {
		for (int i = 0; i < tree_size; ++i) {
			float val = get_vli(bits);
			if (val && get_bit(bits))
				val = -val;
			input[j+3*i] = val;
		}
	}
	close_reader(bits);
	struct image *output = new_image(argv[2], length, length);
	for (int i = 0; i < 3; ++i)
		doit(output->buffer+i, input+i, 3, 0, depth, quant[i]);
	if (mode)
		rgb_image(output);
	if (!write_ppm(output))
		return 1;
	return 0;
}

