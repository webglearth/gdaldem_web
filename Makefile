
GDAL_CFLAGS = `gdal-config --cflags`
GDAL_LDFLAGS = `gdal-config --libs`

CC = gcc
CFLAGS = -Wall -Wextra -ansi -pedantic -std=c99 -O2 $(GDAL_CFLAGS)
LDFLAGS = $(GDAL_LDFLAGS)

all: gdaldem_web

clean:
	rm -f gdaldem_web

