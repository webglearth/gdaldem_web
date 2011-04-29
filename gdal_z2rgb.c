
/*
 * gdal_z2rgb
 * Copyright (C) 2011 Klokan Technologies GmbH (info@klokantech.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * See <http://www.gnu.org/licenses/gpl.html> for the full text.
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cpl_conv.h>
#include <cpl_string.h>
#include <gdal.h>

static char *USAGE =
    "USAGE: gdal_z2rgb -help | OPTIONS src dst\n"
    "OPTIONS: [-b band] [-co \"NAME=VALUE\"] [-nodata own|num num]\n"
    "         [-of format] [-scale min max] [-r resolution]";

int main(int argc, char *argv[])
{
    /* options */
    char *drv_name = "gtiff";
    char **dst_opts = NULL;
    char *dst_path;
    char *src_path;
    float resolution = 1.0f;
    float src_nodata = -FLT_MAX;
    int band_no = 1;
    int dst_nodata = 0;
    int has_nodata = 0;
    int scale_min = -12000;
    int scale_max = 10000;
    int user_nodata = 0;

    /* resources */
    int ret = 1;
    float *src_buf = NULL;
    GDALDatasetH dst = NULL;
    GDALDatasetH src = NULL;
    unsigned char *dst_buf[3] = {NULL, NULL, NULL};

    /* variables */
    CPLErr err;
    double geotrans[6];
    float n_blks;
    float scale_range;
    GDALDriverH drv;
    GDALRasterBandH dst_band;
    GDALRasterBandH src_band;
    int blk_area;
    int blk_x_size;
    int blk_y_size;
    int ds_x_size;
    int ds_y_size;
    int i, n, x, y;

    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-help") == 0) {
            puts(USAGE);
            ret = 0;
            goto end;
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            band_no = atoi(argv[i + 1]);
            i += 2;
        } else if (strcmp(argv[i], "-co") == 0 && i + 1 < argc) {
            dst_opts = CSLAddString(dst_opts, argv[i + 1]);
            i += 2;
        } else if (strcmp(argv[i], "-nodata") == 0 && i + 2 < argc) {
            if (strcmp(argv[i + 1], "own") == 0) {
                user_nodata = 0;
            } else {
                user_nodata = 1;
                src_nodata = atof(argv[i + 1]);
            }
            has_nodata = 1;
            dst_nodata = atoi(argv[i + 2]);
            i += 3;
        } else if (strcmp(argv[i], "-of") == 0 && i + 1 < argc) {
            drv_name = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            resolution = atof(argv[i + 1]);
            i += 2;
        } else if (strcmp(argv[i], "-scale") == 0 && i + 2 < argc) {
            scale_min = atoi(argv[i + 1]);
            scale_max = atoi(argv[i + 2]);
            i += 3;
       } else {
            break;
        }
    }
    if (argc != i + 2) {
        puts(USAGE);
        goto end;
    }
    src_path = argv[i];
    dst_path = argv[i + 1];
    scale_range = scale_max - scale_min + 1;

    if (scale_range <= 0.0f) {
        fprintf(stderr, "Invalid scale\n");
        goto end;
    }
    if (has_nodata && (dst_nodata < scale_min || scale_max < dst_nodata)) {
        fprintf(stderr, "Destination NODATA are outside scale range\n");
        goto end;
    }
    if (resolution <= 0.0f) {
        fprintf(stderr, "Invalid resolution\n");
        goto end;
    }
    if (CPLCheckForFile(dst_path, NULL)) {
        fprintf(stderr, "%s: File exists\n", dst_path);
        goto end;
    }

    GDALAllRegister();
    drv = GDALGetDriverByName(drv_name);
    if (drv == NULL) {
        fprintf(stderr, "%s: Invalid driver name\n", drv_name);
        goto end;
    }

    src = GDALOpen(src_path, GA_ReadOnly);
    if (src == NULL) {
        fprintf(stderr, "%s: Can't open", src_path);
        goto end;
    }
    if (band_no < 1 || GDALGetRasterCount(src) < band_no) {
        fprintf(stderr, "Invalid band number %d\n", band_no);
        goto end;
    }

    src_band = GDALGetRasterBand(src, band_no);
    ds_x_size = GDALGetRasterXSize(src);
    ds_y_size = GDALGetRasterYSize(src);
    if (!user_nodata)
        src_nodata = GDALGetRasterNoDataValue(src_band, &has_nodata);

    dst = GDALCreate(drv, dst_path, ds_x_size, ds_y_size, 3, GDT_Byte, dst_opts);
    if (dst == NULL) {
        fprintf(stderr, "%s: Can't create", dst_path);
        goto end;
    }
    if (GDALSetProjection(dst, GDALGetProjectionRef(src)) != CE_None) {
        fprintf(stderr, "%s: Can't set projection\n", dst_path);
        goto end;
    }
    if (GDALGetGeoTransform(src, geotrans) != CE_None) {
        fprintf(stderr, "%s: Can't get geotransformation\n", src_path);
        goto end;
    }
    if (GDALSetGeoTransform(dst, geotrans) != CE_None) {
        fprintf(stderr, "%s: Can't set geotransformation\n", dst_path);
        goto end;
    }

    dst_band = GDALGetRasterBand(dst, 1);
    GDALGetBlockSize(dst_band, &blk_x_size, &blk_y_size);
    blk_area = blk_x_size * blk_y_size;
    n_blks = ((ds_x_size + blk_x_size - 1) / blk_x_size) *
             ((ds_y_size + blk_y_size - 1) / blk_y_size);

    GDALSetCacheMax(64 << 20);
    src_buf = malloc(sizeof(float) * blk_area);
    if (src_buf == NULL) {
        perror("malloc");
        goto end;
    }
    dst_buf[0] = malloc(blk_area * 3);
    if (dst_buf[0] == NULL) {
        perror("malloc");
        goto end;
    }

    printf("SRC\t%s\nDST\t%s\n", src_path, dst_path);
    printf("SCALE\t%d ... %d m\n", scale_min, scale_max);
    if (has_nodata)
        printf("NODATA\t%f -> %d m\n", src_nodata, dst_nodata);

    n = 0;
    for (y = 0; y < ds_y_size; y += blk_y_size)
    for (x = 0; x < ds_x_size; x += blk_x_size) {
        int area, x_size, y_size;

        x_size = MIN(ds_x_size, x + blk_x_size) - x;
        y_size = MIN(ds_y_size, y + blk_y_size) - y;
        area = x_size * y_size;

        err = GDALRasterIO(src_band, GF_Read, x, y, x_size, y_size,
                           src_buf, x_size, y_size, GDT_Float32, 0, 0);
        if (err != CE_None) {
            fprintf(stderr, "%s: Can't read [%d;%d] %dx%d\n",
                    src_path, x, y, x_size, y_size);
            goto end;
        }

        dst_buf[1] = dst_buf[0] + area;
        dst_buf[2] = dst_buf[1] + area;
        for (i = 0; i < area; i++) {
            float e, v;
            e = src_buf[i];
            if (has_nodata && e == src_nodata)
                e = dst_nodata;
            else
                e *= resolution;
            v = 256.0f * (e - scale_min) / scale_range;
            dst_buf[0][i] = floor(v);
            dst_buf[1][i] = ((int) floor(v * 256.0f)) % 256;
            dst_buf[2][i] = 0;
        }

        err = GDALDatasetRasterIO(dst, GF_Write, x, y, x_size, y_size,
                                  dst_buf[0], x_size, y_size,
                                  GDT_Byte, 3, NULL, 0, 0, 0);
        if (err != CE_None) {
            fprintf(stderr, "%s: Can't write [%d;%d] %dx%d\n",
                    dst_path, x, y, x_size, y_size);
            goto end;
        }

        n++;
        GDALTermProgress(n / n_blks, NULL, NULL);
    }

    ret = 0;
end:
    if (src != NULL)
        GDALClose(src);
    if (dst != NULL)
        GDALClose(dst);
    CSLDestroy(dst_opts);
    free(src_buf);
    free(dst_buf[0]);
    return ret;
}

