CFLAGS = -std=c99 -W -Wall -O3 -D_GNU_SOURCE=1 -g -fsanitize=address
LDLIBS = -lm
RM = rm -f

all: lqtenc lqtdec

test: lqtenc lqtdec
	./lqtenc input.ppm /dev/stdout | ./lqtdec /dev/stdin output.ppm

lqtenc: src/encode.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

lqtdec: src/decode.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

clean:
	$(RM) lqtenc lqtdec

