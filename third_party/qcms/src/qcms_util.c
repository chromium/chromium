// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcmsint.h"

#include <math.h>

typedef struct _qcms_coords {
    float x;
    float y;
} qcms_coords;

typedef struct _qcms_triangle {
    qcms_coords verticies[3];
} qcms_triangle;

#define NTSC_1953_GAMUT_SIZE    0.1582

static qcms_triangle get_profile_triangle(qcms_profile *profile)
{
    float sumRed = s15Fixed16Number_to_float(profile->redColorant.X) +
                   s15Fixed16Number_to_float(profile->redColorant.Y) +
                   s15Fixed16Number_to_float(profile->redColorant.Z);
    float xRed = s15Fixed16Number_to_float(profile->redColorant.X) / sumRed;
    float yRed = s15Fixed16Number_to_float(profile->redColorant.Y) / sumRed;

    float sumGreen = s15Fixed16Number_to_float(profile->greenColorant.X) +
                     s15Fixed16Number_to_float(profile->greenColorant.Y) +
                     s15Fixed16Number_to_float(profile->greenColorant.Z);
    float xGreen = s15Fixed16Number_to_float(profile->greenColorant.X) / sumGreen;
    float yGreen = s15Fixed16Number_to_float(profile->greenColorant.Y) / sumGreen;

    float sumBlue = s15Fixed16Number_to_float(profile->blueColorant.X) +
                    s15Fixed16Number_to_float(profile->blueColorant.Y) +
                    s15Fixed16Number_to_float(profile->blueColorant.Z);
    float xBlue = s15Fixed16Number_to_float(profile->blueColorant.X) / sumBlue;
    float yBlue = s15Fixed16Number_to_float(profile->blueColorant.Y) / sumBlue;

    qcms_triangle triangle = {{{xRed, yRed}, {xGreen, yGreen}, {xBlue, yBlue}}};
    return triangle;
}

static float get_triangle_area(const qcms_triangle candidate)
{
    float xRed = candidate.verticies[0].x;
    float yRed = candidate.verticies[0].y;
    float xGreen = candidate.verticies[1].x;
    float yGreen = candidate.verticies[1].y;
    float xBlue = candidate.verticies[2].x;
    float yBlue = candidate.verticies[2].y;

    float area = fabs((xRed - xBlue) * (yGreen - yBlue) - (xGreen - xBlue) * (yRed - yBlue)) / 2;
    return area;
}

static float get_ntsc_gamut_metric_area(const qcms_triangle candidate)
{
    float area = get_triangle_area(candidate);
    return area * 100 / NTSC_1953_GAMUT_SIZE;
}

float qcms_profile_ntsc_relative_gamut_size(qcms_profile *profile)
{
    qcms_triangle triangle = get_profile_triangle(profile);
    return get_ntsc_gamut_metric_area(triangle);
}


