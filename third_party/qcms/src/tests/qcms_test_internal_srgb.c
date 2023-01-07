// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcms.h"
#include "qcms_test_util.h"

#include <assert.h>
#include <math.h> // sqrt
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef DISPLAY_DEVICE_PROFILE
#define DISPLAY_DEVICE_PROFILE 0x6d6e7472 // 'mntr'
#endif

// D50 adapted color primaries of the internal sRGB color profile.
static s15Fixed16Number sRGB_reference[3][3] = {
    { 0x06fa0, 0x06296, 0x024a0 }, // ( 0.436035, 0.385101, 0.143066 )
    { 0x038f2, 0x0b789, 0x00f85 }, // ( 0.222443, 0.716934, 0.060623 )
    { 0x0038f, 0x018da, 0x0b6c4 }, // ( 0.013901, 0.097076, 0.713928 )
};

// Reference media white point of the sRGB IEC61966-2.1 color profile.
static struct XYZNumber D65 = {
    0xf351, 0x10000, 0x116cc // ( 0.950455, 1.000000, 1.089050 )
};

static void check_profile_description(qcms_profile *profile)
{
    printf("Test profile description:\n");

    const char* description = qcms_profile_get_description(profile);
    printf("description=[%s]\n\n", description);
}

static void check_profile_pcs_white_point(const qcms_profile *profile)
{
    float rX = s15Fixed16Number_to_float(profile->redColorant.X);
    float gX = s15Fixed16Number_to_float(profile->greenColorant.X);
    float bX = s15Fixed16Number_to_float(profile->blueColorant.X);
    float rY = s15Fixed16Number_to_float(profile->redColorant.Y);
    float gY = s15Fixed16Number_to_float(profile->greenColorant.Y);
    float bY = s15Fixed16Number_to_float(profile->blueColorant.Y);
    float rZ = s15Fixed16Number_to_float(profile->redColorant.Z);
    float gZ = s15Fixed16Number_to_float(profile->greenColorant.Z);
    float bZ = s15Fixed16Number_to_float(profile->blueColorant.Z);

    printf("Test PCS white point against expected D50 XYZ values\n");

    float X = rX + gX + bX;
    float Y = rY + gY + bY;
    float Z = rZ + gZ + bZ;

    float x = X / (X + Y + Z);
    float y = Y / (X + Y + Z);

    printf("Computed profile D50 White point xyY = [%.6f %.6f %.6f]\n", x, y, Y);

    float xerr = x - 0.345702915; // Compute error to ICC spec D50 xyY.
    float yerr = y - 0.358538597;
    float Yerr = Y - 1.000000000;

    printf("D50 white point error = %.6f\n\n", (float)
           sqrt((xerr * xerr) + (yerr * yerr) + (Yerr * Yerr)));
}

static void check_profile_media_white_point(const qcms_profile *profile)
{
    int errX = profile->mediaWhitePoint.X - D65.X;
    int errY = profile->mediaWhitePoint.Y - D65.Y;
    int errZ = profile->mediaWhitePoint.Z - D65.Z;

    printf("Test media white point against expected D65 XYZ values\n");
    printf("Internal profile D65 values = [0x%X, 0x%X, 0x%X]\n",
           profile->mediaWhitePoint.X, profile->mediaWhitePoint.Y, profile->mediaWhitePoint.Z);
    printf("D65 media white point error = [%d, %d, %d]\n\n", errX, errY, errZ);
}

static s15Fixed16Number check_profile_primaries(const qcms_profile *profile)
{
    s15Fixed16Number sRGB_internal[3][3];
    s15Fixed16Number primary_error;
    int i, j;

    printf("Test qcms internal sRGB color primaries\n");

    sRGB_internal[0][0] = profile->redColorant.X;
    sRGB_internal[1][0] = profile->redColorant.Y;
    sRGB_internal[2][0] = profile->redColorant.Z;
    sRGB_internal[0][1] = profile->greenColorant.X;
    sRGB_internal[1][1] = profile->greenColorant.Y;
    sRGB_internal[2][1] = profile->greenColorant.Z;
    sRGB_internal[0][2] = profile->blueColorant.X;
    sRGB_internal[1][2] = profile->blueColorant.Y;
    sRGB_internal[2][2] = profile->blueColorant.Z;

    primary_error = 0;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            s15Fixed16Number tmp = sRGB_internal[i][j] - sRGB_reference[i][j];
            printf(" %d", tmp);
            primary_error += abs(tmp);
        }
        printf("\n");
    }

    return primary_error;
}

static int qcms_test_internal_srgb(size_t width,
        size_t height,
        int iterations,
        const char *in_path,
        const char *out_path,
        const int force_software)
{
    s15Fixed16Number primary_error;

    qcms_profile *profile = qcms_profile_sRGB();

    assert(profile->class == DISPLAY_DEVICE_PROFILE);
    assert(profile->rendering_intent == QCMS_INTENT_PERCEPTUAL);
    assert(profile->color_space == RGB_SIGNATURE);
    assert(profile->pcs == XYZ_SIGNATURE);

    if (qcms_profile_is_bogus(profile)) {
        fprintf(stderr, "Failure: the internal sRGB profile failed the bogus profile check\n");
        qcms_profile_release(profile);
        return -1;
    }

    // Compute tristimulus matrix error.
    primary_error = check_profile_primaries(profile);
    printf("Total primary error = 0x%x [%.6f]\n\n", primary_error, primary_error / 65536.0);

    // Verify media white point correctness.
    check_profile_media_white_point(profile);

    // Verify PCS white point correctness.
    check_profile_pcs_white_point(profile);

    // Output profile description.
    check_profile_description(profile);

    qcms_profile_release(profile);
    return primary_error;
}

struct qcms_test_case qcms_test_internal_srgb_info = {
        "qcms_test_internal_srgb",
        qcms_test_internal_srgb,
        QCMS_TEST_DISABLED
};
