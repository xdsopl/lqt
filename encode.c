/*
Encoder for lossy image compression based on the quadtree data structure

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
	doit(output+pixels*stride, input, stride, level+1, depth, quant);
	for (int j = 0; j < length; ++j) {
		for (int i = 0; i < length; ++i) {
			float sum = 0.f;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					sum += output[(pixels+length*2*(j*2+y)+i*2+x)*stride];
			float avg = sum / 4.f;
			output[(length*j+i)*stride] = avg;
			for (int y = 0; y < 2; ++y)
				for (int x = 0; x < 2; ++x)
					output[(pixels+length*2*(j*2+y)+i*2+x)*stride] -= avg;
		}
	}
	if (level == 0)
		output[0] = nearbyintf((quant<<depth) * output[0]);
	quant <<= depth - level - 1;
	for (int i = 0; i < 4 * pixels; ++i)
		output[(pixels+i)*stride] = nearbyintf(quant * output[(pixels+i)*stride]);
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
	float *output = malloc(sizeof(float) * 3 * tree_size);
	if (mode)
		ycbcr_image(input);
	for (int i = 0; i < 3; ++i)
		doit(output+i, input->buffer+i, 3, 0, depth, quant[i]);
	struct bits *bits = bits_writer(argv[2]);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_vli(bits, depth);
	for (int i = 0; i < 3; ++i)
		put_vli(bits, quant[i]);
	for (int j = 0; j < 3; ++j) {
		for (int i = 0; i < tree_size; ++i) {
			put_vli(bits, fabsf(output[j+3*i]));
			if (output[j+3*i])
				put_bit(bits, output[j+3*i] < 0.f);
		}
	}
	close_writer(bits);
	return 0;
}

