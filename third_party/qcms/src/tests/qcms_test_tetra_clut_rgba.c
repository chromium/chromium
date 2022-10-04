// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"
#include "timing.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External qcms tetra clut interpolators.

extern void qcms_transform_data_tetra_clut_rgba(qcms_transform *transform,
                                                unsigned char *src,
                                                unsigned char *dest,
                                                size_t length,
                                                qcms_format_type output_format);

#ifdef SSE2_ENABLE
extern void qcms_transform_data_tetra_clut_rgba_sse2(qcms_transform *transform,
                                                     unsigned char *src,
                                                     unsigned char *dest,
                                                     size_t length,
                                                     qcms_format_type output_format);
#else
void qcms_transform_data_tetra_clut_rgba_dummy(qcms_transform *transform,
                                               unsigned char *src,
                                               unsigned char *dest,
                                               size_t length,
                                               qcms_format_type output_format)
{
    (void)(transform);
    (void)(src);
    (void)(dest);
    (void)(length);
    (void)(output_format);
}
#endif

static float *create_lut(size_t lutSize)
{
    float *lut = malloc(lutSize * sizeof(float));
    size_t i;

    for (i = 0; i < lutSize; ++i) {
        lut[i] = (rand() & 255) * (1.0f / 255.0f);
    }

    return lut;
}

static int diffs;

static int validate(unsigned char *dst0, unsigned char *dst1, size_t length, int limit, const size_t pixel_size)
{
    size_t bytes = length * pixel_size;
    size_t i;

    // Compare dst0/dst0 byte-by-byte, allowing for minor differences due
    // to SSE rounding modes (controlled by the limit argument).

    if (limit < 0)
        limit = 255; // Ignore all differences.

    for (diffs = 0, i = 0; i < bytes; ++i) {
        if (abs((int)dst0[i] - (int)dst1[i]) > limit) {
            ++diffs;
        }
    }

    return !diffs;
}

static int qcms_test_tetra_clut_rgba(size_t width,
        size_t height,
        int iterations,
        const char *in_profile,
        const char *out_profile,
        const int force_software)
{
    qcms_transform transform0, transform1;
    qcms_format_type format = {2, 0};
    uint16_t samples = 33;
    size_t lutSize;
    float *lut0, *lut1;

    const size_t length = width * height;
    const size_t pixel_size = 4;

    double time0, time1;
    int i;

    printf("Test qcms clut transforms for %d iterations\n", iterations);
    printf("Test image size %u x %u pixels\n", (unsigned) width, (unsigned) height);
    fflush(stdout);

    srand(0);
    seconds();

    memset(&transform0, 0, sizeof(transform0));
    memset(&transform1, 0, sizeof(transform1));

    transform0.grid_size = samples;
    transform1.grid_size = samples;

    transform0.transform_flags = 0;
    transform1.transform_flags = 0;

    lutSize = 3 * samples * samples * samples;
    lut0 = create_lut(lutSize);
    lut1 = (float *)malloc(lutSize * sizeof(float));
    memcpy(lut1, lut0, lutSize * sizeof(float));

    transform0.r_clut = &lut0[0];
    transform0.g_clut = &lut0[1];
    transform0.b_clut = &lut0[2];

    transform1.r_clut = &lut1[0];
    transform1.g_clut = &lut1[1];
    transform1.b_clut = &lut1[2];

    // Re-generate and use different data sources during the iteration loop
    // to avoid compiler / cache optimizations that may affect performance.

    time0 = 0.0;
    time1 = 0.0;

    for (i = 0; i < iterations; ++i) {
        unsigned char *src0 = (unsigned char *)calloc(length, pixel_size);
        unsigned char *src1 = (unsigned char *)calloc(length, pixel_size);
        unsigned char *dst0 = (unsigned char *)calloc(length, pixel_size);
        unsigned char *dst1 = (unsigned char *)calloc(length, pixel_size);

        generate_source_uint8_t(src0, length, pixel_size);
        memcpy(src1, src0, length * pixel_size);

#define TRANSFORM_TEST0 qcms_transform_data_tetra_clut_rgba
#ifdef SSE2_ENABLE
#define TRANSFORM_TEST1 qcms_transform_data_tetra_clut_rgba_sse2
#else
#define TRANSFORM_TEST1 qcms_transform_data_tetra_clut_rgba_dummy
#endif

        TIME(TRANSFORM_TEST0(&transform0, src0, dst0, length, format), &time0);
        TIME(TRANSFORM_TEST1(&transform1, src1, dst1, length, format), &time1);

        if (!validate(dst0, dst1, length, 0, pixel_size)) {
            fprintf(stderr, "Invalid transform output: %d diffs\n", diffs);
        }

        free(src0);
        free(src1);
        free(dst0);
        free(dst1);
    }

#define STRINGIZE(s) #s
#define STRING(s) STRINGIZE(s)

    printf("%.6lf (avg %.6lf) seconds " STRING(TRANSFORM_TEST0) "\n",
            time0, time0 / iterations);
    printf("%.6lf (avg %.6lf) seconds " STRING(TRANSFORM_TEST1) "\n",
            time1, time1 / iterations);
    printf("%.6lf speedup after %d iterations\n\n",
            time0 / time1, iterations);

    free(lut0);
    free(lut1);

    return diffs;
}

struct qcms_test_case qcms_test_tetra_clut_rgba_info = {
        "qcms_test_tetra_clut_rgba",
        qcms_test_tetra_clut_rgba,
        QCMS_TEST_DISABLED
};
