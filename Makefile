
GDAL_CFLAGS = `gdal-config --cflags`
GDAL_LDFLAGS = `gdal-config --libs`

CC = gcc
CFLAGS = -Wall -Wextra -ansi -pedantic -std=c99 -O2 $(GDAL_CFLAGS)
LDFLAGS = $(GDAL_LDFLAGS)

all: gdal_z2rgb

clean:
	rm -f gdal_z2rgb

