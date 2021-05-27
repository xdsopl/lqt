/*
Encoder for lossless image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "rle.h"
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

void reorder(int *tree, int *buffer, int length)
{
	for (int len = 2, size = 4, *level = tree+1; len <= length; level += size, len *= 2, size = len*len) {
		for (int i = 0; i < size; ++i)
			buffer[i] = level[i];
		for (int i = 0; i < size; ++i)
			level[i] = buffer[hilbert(len, i)];
	}
}

int encode(struct rle_writer *rle, int *val, int num, int plane)
{
	int bit_mask = 1 << plane;
	int int_bits = sizeof(int) * 8;
	int sgn_pos = int_bits - 1;
	int sig_pos = int_bits - 2;
	int sgn_mask = 1 << sgn_pos;
	int sig_mask = 1 << sig_pos;
	int ret = put_rle(rle, 1);
	if (ret)
		return ret;
	for (int i = 0; i < num; ++i) {
		if (val[i] & sig_mask) {
			int bit = val[i] & bit_mask;
			int ret = rle_put_bit(rle, bit);
			if (ret)
				return ret;
		}
	}
	for (int i = 0; i < num; ++i) {
		if (!(val[i] & sig_mask)) {
			int bit = val[i] & bit_mask;
			int ret = put_rle(rle, bit);
			if (ret)
				return ret;
			if (bit) {
				int ret = rle_put_bit(rle, val[i] & sgn_mask);
				if (ret)
					return ret;
				val[i] |= sig_mask;
			}
		}
	}
	return 0;
}

void encode_root(struct vli_writer *vli, int *root)
{
	put_vli(vli, abs(*root));
	if (*root)
		vli_put_bit(vli, *root < 0);
}

int ilog2(int x)
{
	int l = -1;
	for (; x > 0; x /= 2)
		++l;
	return l;
}

int process(int *val, int num)
{
	int max = 0;
	int int_bits = sizeof(int) * 8;
	int sgn_pos = int_bits - 1;
	int sig_pos = int_bits - 2;
	int mix_mask = (1 << sgn_pos) | (1 << sig_pos);
	for (int i = 0; i < num; ++i) {
		int sgn = val[i] < 0;
		int mag = abs(val[i]);
		if (max < mag)
			max = mag;
		val[i] = (sgn << sgn_pos) | (mag & ~mix_mask);
	}
	return 1 + ilog2(max);
}

int main(int argc, char **argv)
{
	if (argc != 3 && argc != 4 && argc != 5) {
		fprintf(stderr, "usage: %s input.ppm output.lqt [MODE] [CAPACITY]\n", argv[0]);
		return 1;
	}
	int mode = 1;
	if (argc >= 4)
		mode = atoi(argv[3]);
	struct image *image = read_ppm(argv[1]);
	if (!image)
		return 1;
	int width = image->width;
	int height = image->height;
	int length = 1;
	int depth = 0;
	while (length < width || length < height)
		length = 1 << ++depth;
	int pixels = length * length;
	if (mode) {
		rct_image(image);
		for (int i = 0; i < width * height; ++i)
			image->buffer[3*i] -= 128;
	} else {
		for (int i = 0; i < 3 * width * height; ++i)
			image->buffer[i] -= 128;
	}
	int *input = malloc(sizeof(int) * pixels);
	int tree_size = (pixels * 4 - 1) / 3;
	int *tree = malloc(sizeof(int) * 3 * tree_size);
	for (int chan = 0; chan < 3; ++chan) {
		copy(input, image->buffer+chan, width, height, length, 3);
		doit(tree+chan*tree_size, input, 0, depth);
		reorder(tree+chan*tree_size, input, length);
	}
	int planes[3] = { 0 };
	for (int chan = 0; chan < 3; ++chan) {
		int cnt = process(tree+chan*tree_size+1, tree_size-1);
		if (planes[chan] < cnt)
			planes[chan] = cnt;
	}
	free(input);
	delete_image(image);
	int capacity = 1 << 24;
	if (argc >= 5)
		capacity = atoi(argv[4]);
	struct bits_writer *bits = bits_writer(argv[2], capacity);
	if (!bits)
		return 1;
	struct vli_writer *vli = vli_writer(bits);
	vli_put_bit(vli, mode);
	put_vli(vli, width);
	put_vli(vli, height);
	for (int chan = 0; chan < 3; ++chan)
		encode_root(vli, tree+chan*tree_size);
	for (int chan = 0; chan < 3; ++chan)
		put_vli(vli, planes[chan]);
	struct rle_writer *rle = rle_writer(vli);
	int planes_max = 0;
	for (int chan = 0; chan < 3; ++chan)
		if (planes_max < planes[chan])
			planes_max = planes[chan];
	int maximum = depth > planes_max ? depth : planes_max;
	int layers_max = 2 * maximum - 1;
	for (int layers = 0; layers < layers_max; ++layers) {
		for (int layer = 0, len = 2, *level = tree+1; len <= length && layer <= layers; level += len*len, len *= 2, ++layer) {
			for (int chan = 0; chan < 1; ++chan) {
				int plane = planes_max-1 - (layers-layer);
				if (plane < 0 || plane >= planes[chan])
					continue;
				if (encode(rle, level+chan*tree_size, len*len, plane))
					goto end;
			}
		}
		for (int layer = 0, len = 2, *level = tree+1; len <= length && layer <= layers; level += len*len, len *= 2, ++layer) {
			for (int chan = 1; chan < 3; ++chan) {
				int plane = planes_max-1 - (layers-layer);
				if (plane < 0 || plane >= planes[chan])
					continue;
				if (encode(rle, level+chan*tree_size, len*len, plane))
					goto end;
			}
		}
	}
	rle_flush(rle);
end:
	delete_rle_writer(rle);
	delete_vli_writer(vli);
	free(tree);
	int cnt = bits_count(bits);
	int bytes = (cnt + 7) / 8;
	int kib = (bytes + 512) / 1024;
	fprintf(stderr, "%d bits (%d KiB) encoded\n", cnt, kib);
	close_writer(bits);
	return 0;
}

