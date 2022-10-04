// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct color_checker_chart {
    char* name;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct color_checker_chart adobe_munsell[24] = {
        { "Dark Skin", 106, 81, 67, 255 },
        { "Light Skin", 182, 149, 130, 255 },
        { "Blue Sky", 103, 122, 154, 255 },
        { "Foliage", 95, 108, 69, 255 },
        { "Blue Flower", 129, 128, 174, 255 },
        { "Bluish Green", 133, 189, 170, 255 },
        { "Orange", 194, 121, 48, 255 },
        { "Purplish Blue", 79, 91, 162, 255 },
        { "Moderate Red", 170, 85, 97, 255 },
        { "Purple", 84, 62, 105, 255 },
        { "Yellow Green", 167, 186, 73, 255 },
        { "Orange Yellow", 213, 162, 57, 255 },
        { "Blue", 54, 62, 149, 255 },
        { "Green", 101, 148, 76, 255 },
        { "Red", 152, 48, 58, 255 },
        { "Yellow", 228, 199, 55, 255 },
        { "Magenta", 164, 83, 144, 255 },
        { "Cyan", 63, 134, 163, 255 },
        { "White", 242, 241, 236, 255 },
        { "Neutral 8", 200, 200, 199, 255 },
        { "Neutral 6.5", 159, 160, 159, 255 },
        { "Neutral 5", 122, 121, 120, 255 },
        { "Neutral 3.5", 84, 84, 84, 255 },
        { "Black", 53, 53, 53, 255 },
};

struct color_checker_chart srgb_munsell[24] = {
        { "Dark Skin", 115, 80, 64, 255 },
        { "Light Skin", 195, 151, 130, 255 },
        { "Blue Sky", 94, 123, 156, 255 },
        { "Foliage", 88, 108, 65, 255 },
        { "Blue Flower", 130, 129, 177, 255 },
        { "Bluish Green", 100, 190, 171, 255 },
        { "Orange", 217, 122, 37, 255 },
        { "Purplish Blue", 72, 91, 165, 255 },
        { "Moderate Red", 194, 84, 98, 255 },
        { "Purple", 91, 59, 107, 255 },
        { "Yellow Green", 160, 188, 60, 255 },
        { "Orange Yellow", 230, 163, 42, 255 },
        { "Blue", 46, 60, 153, 255 },
        { "Green", 71, 150, 69, 255 },
        { "Red", 177, 44, 56, 255 },
        { "Yellow", 238, 200, 27, 255 },
        { "Magenta", 187, 82, 148, 255 },
        { "Cyan", /* -49 */ 0, 135, 166, 255 },
        { "White", 243, 242, 237, 255 },
        { "Neutral 8",201, 201, 201, 255 },
        { "Neutral 6.5", 161, 161, 161, 255 },
        { "Neutral 5",122, 122, 121, 255 },
        { "Neutral 3.5", 83, 83, 83, 255 },
        { "Black", 50, 49, 50, 255 },
};

extern void qcms_transform_data_rgba_out_lut_precache(qcms_transform *transform,
        unsigned char *src,
        unsigned char *dest,
        size_t length,
        qcms_format_type output_format);

static qcms_bool invalid_rgb_color_profile(qcms_profile *profile)
{
    return rgbData != qcms_profile_get_color_space(profile) || qcms_profile_is_bogus(profile);
}

static int color_error(struct color_checker_chart cx, struct color_checker_chart cy)
{
    int dr = cx.r - cy.r;
    int dg = cx.g - cy.g;
    int db = cx.b - cy.b;

    return round(sqrt((dr * dr) + (dg * dg) + (db * db)));
}

static qcms_profile* open_profile_from_path(const char *path)
{
    if (strcmp(path, "internal-srgb") != 0)
        return qcms_profile_from_path(path);
    return qcms_profile_sRGB();
}

static int qcms_test_munsell(size_t width,
        size_t height,
        int iterations,
        const char *in_path,
        const char *out_path,
        const int force_software)
{
    qcms_profile *in_profile = NULL;
    qcms_profile *out_profile = NULL;
    qcms_format_type format = {0, 2}; // RGBA
    qcms_transform *transform;

    struct color_checker_chart *source_munsell = NULL;
    struct color_checker_chart *reference_munsell = NULL;
    struct color_checker_chart destination_munsell[24];

    char file_name[256];
    FILE *output;
    int dE[24];
    float rmse;

    int i;

    printf("Test qcms data transform accuracy using Munsell colors\n");
    fflush(stdout);

    if (in_path == NULL || out_path == NULL) {
        fprintf(stderr, "%s: please provide valid ICC profiles via -i/o options\n", __FUNCTION__);
        return EXIT_FAILURE;
    }

    in_profile = open_profile_from_path(in_path);
    if (!in_profile || invalid_rgb_color_profile(in_profile)) {
        fprintf(stderr, "Invalid input profile\n");
        return EXIT_FAILURE;
    }

    source_munsell = srgb_munsell;
    if (strstr(in_profile->description, "Adobe") != NULL) {
        source_munsell = adobe_munsell;
    }

    printf("Input profile %s\n", in_profile->description);

    out_profile = open_profile_from_path(out_path);
    if (!out_profile || invalid_rgb_color_profile(out_profile)) {
        fprintf(stderr, "Invalid output profile\n");
        return EXIT_FAILURE;
    }

    reference_munsell = srgb_munsell;
    if (strstr(out_profile->description, "Adobe") != NULL) {
        reference_munsell = adobe_munsell;
    }

    printf("Output profile %s (using qcms precache)\n", out_profile->description);
    qcms_profile_precache_output_transform(out_profile);

    transform = qcms_transform_create(in_profile, QCMS_DATA_RGBA_8, out_profile, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform) {
        fprintf(stderr, "Failed to create color transform\n");
        return EXIT_FAILURE;
    } else if (force_software) {
        transform->transform_fn = qcms_transform_data_rgba_out_lut_precache;
    }

    if (qcms_profile_match(in_profile, out_profile)) {
        printf("Note: input / output profiles match\n");
    }

    rmse = 0.0f;

    for (i = 0; i < 24; i++) {
        transform->transform_fn(transform, &source_munsell[i].r, &destination_munsell[i].r, 1, format);
        dE[i] = color_error(reference_munsell[i], destination_munsell[i]);
        rmse += dE[i] * dE[i];
    }

    rmse = sqrt(rmse / 24);
    printf("RMS color error %.2f\n", rmse);

    // Name and open test result file.
    sprintf(file_name, "qcms-test-%ld-munsell-%s-to-%s-rms-%.3f.csv", (long int)time(NULL), in_profile->description, out_profile->description, rmse);
    // FIXME: remove spaces from the file name?
    output = fopen(file_name, "w");

    // Print headers.
    if (force_software)
        fprintf(output, "Report for: qcms_transform_data_rgba_out_lut_precache\n\n");
    else
        fprintf(output, "Report for: qcms_transform_data_rgba_out_lut_sse2\n\n");

    fprintf(output, "%14s,\t%s,\t%s,\t%s\n\n", "Color,", "Actual,,", "Expected,", "dE");

    // Print results.
    for (i = 0; i < 24; i++) {
        fprintf(output, "%14s,\t%d,%d,%d,\t%d,%d,%d,\t%d\n",
                source_munsell[i].name,
                destination_munsell[i].r, destination_munsell[i].g, destination_munsell[i].b,
                reference_munsell[i].r, reference_munsell[i].g, reference_munsell[i].b,
                dE[i]);
    }

    fprintf(output, "\nRMS color error = %.2f\n", rmse);
    fclose(output);

    printf("Output written to %s\n", file_name);
    return rmse > 0.000001f;
}

struct qcms_test_case qcms_test_munsell_info = {
        "qcms_test_munsell",
        qcms_test_munsell,
        QCMS_TEST_DISABLED
};
