/*
Encoder for lossless image compression based on the quadtree data structure

Copyright 2021 Ahmet Inan <xdsopl@gmail.com>
*/

#include "ppm.h"
#include "vli.h"
#include "bits.h"

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

void traverse(int **output, int *input, int level, int depth, int row, int col)
{
	int length = 1 << level;
	int pixels = length * length;
	*(*output)++ = input[length*row+col];
	if (level == depth)
		return;
	traverse(output, input+pixels, level+1, depth, row*2+0, col*2+0);
	traverse(output, input+pixels, level+1, depth, row*2+0, col*2+1);
	traverse(output, input+pixels, level+1, depth, row*2+1, col*2+0);
	traverse(output, input+pixels, level+1, depth, row*2+1, col*2+1);
}

void reorder(int *output, int *input, int depth)
{
	traverse(&output, input, 0, depth, 0, 0);
}

void encode(struct bits_writer *bits, int *level, int size, int plane, int planes)
{
	int mask = 1 << plane, last = 0;
	for (int i = 0; i < size; ++i) {
		if (level[i] & mask) {
			if (plane == planes-1)
				level[i] = -level[i];
			put_vli(bits, i - last);
			last = i + 1;
		}
	}
	put_vli(bits, size - last);
}

void encode_root(struct bits_writer *bits, int *root)
{
	put_vli(bits, abs(*root));
	if (*root)
		put_bit(bits, *root < 0);
}

int ilog2(int x)
{
	int l = -1;
	for (; x > 0; x /= 2)
		++l;
	return l;
}

int count_planes(int *val, int num)
{
	int max = 0;
	for (int i = 0; i < num; ++i)
		if (max < abs(val[i]))
			max = abs(val[i]);
	return 2 + ilog2(max);
}

int over_capacity(struct bits_writer *bits, int capacity)
{
	int cnt = bits_count(bits);
	if (cnt > capacity) {
		bits_discard(bits);
		fprintf(stderr, "%d bits over capacity, discarding.\n", cnt-capacity);
		return 1;
	}
	bits_flush(bits);
	return 0;
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
	int *temp = malloc(sizeof(int) * tree_size);
	for (int chan = 0; chan < 3; ++chan) {
		copy(input, image->buffer+chan, width, height, length, 3);
		doit(temp, input, 0, depth);
		reorder(tree+chan*tree_size, temp, depth);
	}
	free(temp);
	int planes = 0;
	for (int chan = 0; chan < 3; ++chan) {
		int cnt = count_planes(tree+chan*tree_size+1, tree_size-1);
		if (planes < cnt)
			planes = cnt;
	}
	free(input);
	delete_image(image);
	int capacity = 1 << 24;
	if (argc >= 5)
		capacity = atoi(argv[4]);
	struct bits_writer *bits = bits_writer(argv[2], capacity);
	if (!bits)
		return 1;
	put_bit(bits, mode);
	put_vli(bits, width);
	put_vli(bits, height);
	put_vli(bits, planes);
	for (int chan = 0; chan < 3; ++chan)
		encode_root(bits, tree+chan*tree_size);
	bits_flush(bits);
	for (int plane = planes-1; plane >= 0; --plane) {
		for (int chan = 0; chan < 3; ++chan)
			encode(bits, tree+chan*tree_size+1, tree_size-1, plane, planes);
		if (over_capacity(bits, capacity))
			goto end;
	}
end:
	free(tree);
	int cnt = bits_count(bits);
	int bytes = (cnt + 7) / 8;
	int kib = (bytes + 512) / 1024;
	fprintf(stderr, "%d bits (%d KiB) encoded\n", cnt, kib);
	close_writer(bits);
	return 0;
}

