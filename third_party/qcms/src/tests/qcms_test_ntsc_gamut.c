// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcms_test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static qcms_bool invalid_rgb_color_profile(qcms_profile *profile)
{
    return rgbData != qcms_profile_get_color_space(profile) || qcms_profile_is_bogus(profile);
}

static int qcms_test_ntsc_gamut(size_t width,
        size_t height,
        int iterations,
        const char *input_path,
        const char *referece_path,
        const int force_software)
{
    qcms_profile *input_profile;
    qcms_profile *reference_profile = qcms_profile_sRGB();
    qcms_transform *transform;
    float input_gamut_metric, reference_gamut_metric;

    if (!input_path) {
        fprintf(stderr, "%s: please provide valid ICC profiles via -i/o options\n", __FUNCTION__);
        return EXIT_FAILURE;
    }

    input_profile = qcms_profile_from_path(input_path);
    if (!input_profile || invalid_rgb_color_profile(input_profile)) {
        fprintf(stderr, "Invalid input profile\n");
        return EXIT_FAILURE;
    }

    transform = qcms_transform_create(input_profile, QCMS_DATA_RGBA_8, reference_profile, QCMS_DATA_RGBA_8, QCMS_INTENT_DEFAULT);
    if (!transform) {
        fprintf(stderr, "Could not create transform\n");
        return EXIT_FAILURE;
    }

    if (!(transform->transform_flags & TRANSFORM_FLAG_MATRIX)) {
        fprintf(stderr, "Transform is not matrix\n");
        qcms_transform_release(transform);
        qcms_profile_release(input_profile);
        qcms_profile_release(reference_profile);
        return EXIT_FAILURE;
    }

    printf("NTSC 1953 relative gamut area test\n");

    input_gamut_metric = qcms_profile_ntsc_relative_gamut_size(input_profile);
    printf("Input profile\n\tDescription: %s\n\tNTSC relative gamut area: %.3f %%\n",
            input_profile->description, input_gamut_metric);

    reference_gamut_metric = qcms_profile_ntsc_relative_gamut_size(reference_profile);
    printf("Internal reference profile\n\tDescription: %s\n\tNTSC relative gamut area: %.3f %%\n",
            reference_profile->description, reference_gamut_metric);

    qcms_transform_release(transform);
    qcms_profile_release(input_profile);
    qcms_profile_release(reference_profile);

    return 0;
}

struct qcms_test_case qcms_test_ntsc_gamut_info = {
        "qcms_test_ntsc_gamut",
        qcms_test_ntsc_gamut,
        QCMS_TEST_DISABLED
};
