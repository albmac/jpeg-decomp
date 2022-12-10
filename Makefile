CC = gcc
CFLAGS =  -w -Os -s #size
#CFLAGS = -w -g		#debug

all: jpeg-decomp.c MCU.h
	$(CC) $(CFLAGS) -o jpeg-decomp jpeg-decomp.c

clean:
	rm -f jpeg-decomp
