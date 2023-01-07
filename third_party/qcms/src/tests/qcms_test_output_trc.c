// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PARAMETRIC_CURVE_TYPE 0x70617261 // 'para'

static const float inverse65535 = (float) (1.0 / 65535.0);

extern float clamp_float(float a);

static int get_output_gamma_table(const char *profile_path, uint16_t **table, size_t *size)
{
    qcms_transform *transform;
    qcms_profile *sRGB;
    qcms_profile *target;

    target = qcms_profile_from_path(profile_path);
    if (!target) {
        fprintf(stderr, "Invalid input profile\n");
        return EXIT_FAILURE;
    }

    sRGB = qcms_profile_sRGB();

    transform = qcms_transform_create(sRGB, QCMS_DATA_RGBA_8, target, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform) {
        fprintf(stderr, "Failed to create colour transform\n");
        qcms_profile_release(sRGB);
        qcms_profile_release(target);
        return EXIT_FAILURE;
    }

    *size = qcms_transform_get_output_trc_rgba(transform, target, QCMS_TRC_USHORT, NULL);
    assert(*size >= 256);

    *table = malloc(*size * sizeof(uint16_t) * 4);
    qcms_transform_get_output_trc_rgba(transform, target, QCMS_TRC_USHORT, *table);

    qcms_transform_release(transform);
    qcms_profile_release(sRGB);
    qcms_profile_release(target);

    return 0;
}

static int get_input_gamma_table(const char *profile_path, uint16_t **table, size_t *size)
{
    qcms_transform *transform;
    qcms_profile *source;
    qcms_profile *sRGB;

    source = qcms_profile_from_path(profile_path);
    if (!source) {
        fprintf(stderr, "Invalid input profile\n");
        return EXIT_FAILURE;
    }

    sRGB = qcms_profile_sRGB();

    transform = qcms_transform_create(source, QCMS_DATA_RGBA_8, sRGB, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform) {
        fprintf(stderr, "Failed to create colour transform\n");
        qcms_profile_release(sRGB);
        qcms_profile_release(source);
        return EXIT_FAILURE;
    }

    *size = qcms_transform_get_input_trc_rgba(transform, source, QCMS_TRC_USHORT, NULL);
    assert(*size >= 256);

    *table = calloc(*size, sizeof(uint16_t) * 4);
    qcms_transform_get_input_trc_rgba(transform, source, QCMS_TRC_USHORT, *table);

    qcms_transform_release(transform);
    qcms_profile_release(sRGB);
    qcms_profile_release(source);

    return 0;
}

static int qcms_test_output_trc(size_t width,
        size_t height,
        int iterations,
        const char *in_path,
        const char *out_path,
        const int force_software)
{
    uint16_t *gamma_table_out = NULL;
    size_t output_size = 0;
    qcms_profile *profile;
    long time_stamp = (long)time(NULL);
    char output_file_name[1024];
    float scale_factor;
    size_t i;

    if (!in_path) {
        fprintf(stderr, "%s: please provide valid ICC profiles via -i option\n", __FUNCTION__);
        return EXIT_FAILURE;
    }

    printf("Test color profile gamma curves\n");
    fflush(stdout);

    if (get_output_gamma_table(in_path, &gamma_table_out, &output_size) != 0) {
        fprintf(stderr, "Unable to extract output gamma table\n");
        return EXIT_FAILURE;
    }

    printf("Output gamma table size = %zu\n", output_size);

    profile = qcms_profile_from_path(in_path);
    if (!profile) {
        fprintf(stderr, "Invalid input profile\n");
        free(gamma_table_out);
        return EXIT_FAILURE;
    }

    if (profile->redTRC->type == PARAMETRIC_CURVE_TYPE) {
        // Check the red TRC curve only for now.
        int type = - (int)(profile->redTRC->count + 1);
        uint16_t *gamma_table_in = NULL;
        size_t input_size = 0;
        FILE *output_file;

        printf("Detected parametric curve type = %d\n", profile->redTRC->count);

        if (get_input_gamma_table(in_path, &gamma_table_in, &input_size) != 0) {
            fprintf(stderr, "Failed to compute input gamma table\n");
            qcms_profile_release(profile);
            free(gamma_table_out);
            return EXIT_FAILURE;
        }

        // Write output to stdout and tables into a csv file.
        sprintf(output_file_name, "qcms-test-%ld-parametric-gamma-output-%s.csv",
                time_stamp, profile->description);
        printf("Writing output gamma tables to %s\n", output_file_name);
        output_file = fopen(output_file_name, "w");

        printf("Parametric gamma values for profile %s description [%s]\n",
               in_path, profile->description);
        fprintf(output_file, "Parametric gamma values for profile %s description [%s]\n",
                in_path, profile->description);

        printf("gamma = %.6f, a = %.6f, b = %.6f, c = %.6f, d = %.6f, e = %.6f, f = %.6f\n",
                profile->redTRC->parameter[0], profile->redTRC->parameter[1], profile->redTRC->parameter[2],
                profile->redTRC->parameter[3], profile->redTRC->parameter[4], profile->redTRC->parameter[5],
                profile->redTRC->parameter[6]);

        fprintf(output_file, "gamma, a, b, c, d, e, f\n");
        fprintf(output_file, "%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
                profile->redTRC->parameter[0], profile->redTRC->parameter[1], profile->redTRC->parameter[2],
                profile->redTRC->parameter[3], profile->redTRC->parameter[4], profile->redTRC->parameter[5],
                profile->redTRC->parameter[6]);

        fprintf(output_file, "\nInput curve size: %zu", input_size);
        fprintf(output_file, "\nOutput curve size: %zu", output_size);

        fprintf(output_file, "\n\nInput gamma, Output gamma, LCMS Output gamma, Output gamma error\n");
        // Output gamma curve down-sample factor.
        scale_factor = (float)(output_size - 1) / (input_size - 1);

        for (i = 0; i < input_size; ++i) {
            float input = gamma_table_in[i * 4] * inverse65535;
            size_t out_index = (size_t)floor(i * scale_factor + 0.5);
            float output = gamma_table_out[out_index * 4] * inverse65535;
            float x = out_index / (float)(output_size - 1);
            float reference = clamp_float(evaluate_parametric_curve(type, profile->redTRC->parameter, x));
            float difference = fabs(output - reference);

            fprintf(output_file, "%.6f, %.6f, %6f, %6f\n", input, output, reference, difference);
        }

        fprintf(output_file, "\nNote: the output gamma curves are down-sampled by a factor of %zu / %zu\n",
                output_size, input_size);

        fclose(output_file);
        free(gamma_table_in);
    } else {
        uint16_t *gamma_table_in = NULL;
        size_t input_size = 0;
        FILE *output_file;

        if (get_input_gamma_table(in_path, &gamma_table_in, &input_size) != 0) {
            fprintf(stderr, "Failed to compute input gamma table\n");
            qcms_profile_release(profile);
            free(gamma_table_out);
            return EXIT_FAILURE;
        }

        // Write output to stdout and tables into a csv file.
        sprintf(output_file_name, "qcms-test-%ld-gamma-output-%s.csv",
                time_stamp, profile->description);
        printf("Writing gamma tables to %s\n", output_file_name);
        output_file = fopen(output_file_name, "w");

        printf("Gamma values for profile %s description [%s]\n",
               in_path, profile->description);
        fprintf(output_file, "Gamma values for profile %s description [%s]\n",
                in_path, profile->description);

        if (profile->redTRC->count == 0) {
            printf("Gamma LUT type 0: linear gamma\n");
            fprintf(output_file, "Gamma LUT type 0: linear gamma\n");
        } else if (profile->redTRC->count == 1) {
            float gamma = profile->redTRC->data[0] / 256.0f;
            printf("Gamma LUT type 1: gamma = %.6f\n", gamma);
            fprintf(output_file, "Gamma LUT type 1: gamma = %.6f\n", gamma);
        } else {
            printf("Gamma LUT table size = %u\n", profile->redTRC->count);
            fprintf(output_file, "Gamma LUT table size = %u\n", profile->redTRC->count);
        }

        fprintf(output_file, "\nInput curve size: %zu", input_size);
        fprintf(output_file, "\nOutput curve size: %zu", output_size);

        fprintf(output_file, "\n\nInput gamma, Output gamma\n");
        // Output gamma curve down-sample factor.
        scale_factor = (float)(output_size - 1) / (input_size - 1);

        for (i = 0; i < input_size; ++i) {
            float input = gamma_table_in[i * 4] * inverse65535;
            size_t out_index = (size_t)floor(i * scale_factor + 0.5);
            float output = gamma_table_out[out_index * 4] * inverse65535;

            fprintf(output_file, "%.6f, %.6f\n", input, output);
        }

        fprintf(output_file, "\nNote: the output gamma curves are down-sampled by a factor of %zu / %zu\n",
                output_size, input_size);

        fclose(output_file);
        free(gamma_table_in);
    }

    qcms_profile_release(profile);
    free(gamma_table_out);

    return 0;
}

struct qcms_test_case qcms_test_output_trc_info = {
        "qcms_test_output_trc",
        qcms_test_output_trc,
        QCMS_TEST_DISABLED
};
