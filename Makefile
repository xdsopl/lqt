CFLAGS = -std=c99 -W -Wall -O3 -D_GNU_SOURCE=1 -g
LDLIBS = -lm

all: encode decode

test: encode decode
	./encode input.ppm putput.lqt
	./decode putput.lqt output.ppm

clean:
	rm -f encode decode

