/*
Decoder for lossless image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "rle.h"
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

void reorder(int *tree, int *buffer, int length)
{
	for (int len = 2, size = 4, *level = tree+1; len <= length; level += size, len *= 2, size = len*len) {
		for (int i = 0; i < size; ++i)
			buffer[i] = level[i];
		for (int i = 0; i < size; ++i)
			level[hilbert(len, i)] = buffer[i];
	}
}

int decode(struct rle_reader *rle, int *val, int num, int plane)
{
	int int_bits = sizeof(int) * 8;
	int sgn_pos = int_bits - 1;
	int sig_pos = int_bits - 2;
	int ref_pos = int_bits - 3;
	int sig_mask = 1 << sig_pos;
	int ref_mask = 1 << ref_pos;
	for (int i = 0; i < num; ++i) {
		if (!(val[i] & ref_mask)) {
			int bit = get_rle(rle);
			if (bit < 0)
				return bit;
			val[i] |= bit << plane;
			if (bit) {
				int sgn = rle_get_bit(rle);
				if (sgn < 0)
					return sgn;
				val[i] |= (sgn << sgn_pos) | sig_mask;
			}
		}
	}
	for (int i = 0; i < num; ++i) {
		if (val[i] & ref_mask) {
			int bit = rle_get_bit(rle);
			if (bit < 0)
				return bit;
			val[i] |= bit << plane;
		} else if (val[i] & sig_mask) {
			val[i] ^= sig_mask | ref_mask;
		}
	}
	return 0;
}

int decode_root(struct vli_reader *vli, int *root)
{
	int ret = get_vli(vli);
	if (ret < 0)
		return ret;
	*root = ret;
	if (!ret)
		return 0;
	if ((ret = vli_get_bit(vli)) < 0)
		return ret;
	if (ret)
		*root = - *root;
	return 0;
}

void process(int *val, int num)
{
	int int_bits = sizeof(int) * 8;
	int sgn_pos = int_bits - 1;
	int sig_pos = int_bits - 2;
	int ref_pos = int_bits - 3;
	int sgn_mask = 1 << sgn_pos;
	int sig_mask = 1 << sig_pos;
	int ref_mask = 1 << ref_pos;
	for (int i = 0; i < num; ++i) {
		val[i] &= ~(sig_mask|ref_mask);
		if (val[i] & sgn_mask)
			val[i] = -(val[i]^sgn_mask);
	}
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
	struct vli_reader *vli = vli_reader(bits);
	int mode = vli_get_bit(vli);
	int width = get_vli(vli);
	int height = get_vli(vli);
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
	for (int chan = 0; chan < 3; ++chan)
		if (decode_root(vli, tree+chan*tree_size))
			return 1;
	int planes[3];
	for (int chan = 0; chan < 3; ++chan)
		if ((planes[chan] = get_vli(vli)) < 0)
			return 1;
	struct rle_reader *rle = rle_reader(vli);
	int planes_max = 0;
	for (int chan = 0; chan < 3; ++chan)
		if (planes_max < planes[chan])
			planes_max = planes[chan];
	int maximum = depth > planes_max ? depth : planes_max;
	int layers_max = 2 * maximum - 1;
	for (int layers = 0; layers < layers_max; ++layers) {
		for (int layer = 0, len = 2, *level = tree+1; len <= length; level += len*len, len *= 2, ++layer) {
			for (int chan = 0; chan < 1; ++chan) {
				int plane = planes_max-1 - (layers-layer);
				if (plane < 0 || plane >= planes[chan])
					continue;
				if (decode(rle, level+chan*tree_size, len*len, plane))
					goto end;
			}
		}
		for (int layer = 0, len = 2, *level = tree+1; len <= length; level += len*len, len *= 2, ++layer) {
			for (int chan = 1; chan < 3; ++chan) {
				int plane = planes_max-1 - (layers-layer);
				if (plane < 0 || plane >= planes[chan])
					continue;
				if (decode(rle, level+chan*tree_size, len*len, plane))
					goto end;
			}
		}
	}
end:
	delete_rle_reader(rle);
	delete_vli_reader(vli);
	close_reader(bits);
	for (int chan = 0; chan < 3; ++chan)
		process(tree+chan*tree_size+1, tree_size-1);
	int *output = malloc(sizeof(int) * pixels);
	struct image *image = new_image(argv[2], width, height);
	for (int chan = 0; chan < 3; ++chan) {
		reorder(tree+chan*tree_size, output, length);
		doit(tree+chan*tree_size, output, 0, depth);
		copy(image->buffer+chan, output, width, height, length, 3);
	}
	free(tree);
	free(output);
	if (mode) {
		for (int i = 0; i < width * height; ++i)
			image->buffer[3*i] += 128;
		rgb_image(image);
	} else {
		for (int i = 0; i < 3 * width * height; ++i)
			image->buffer[i] += 128;
	}
	if (!write_ppm(image))
		return 1;
	delete_image(image);
	return 0;
}

