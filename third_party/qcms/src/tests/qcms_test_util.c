// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcms_test_util.h"

#include <math.h>
#include <stdlib.h>

#define MAX_FLOAT_ERROR 0.000001f

// Store random pixel data in the source.
void generate_source_uint8_t(unsigned char *src, const size_t length, const size_t pixel_size)
{
    size_t bytes = length * pixel_size;
    size_t i;

    for (i = 0; i < bytes; ++i) {
        *src++ = rand() & 255;
    }
}

// Parametric Fn using floating point <from lcms/src/cmsgamma.c>: DefaultEvalParametricFn
float evaluate_parametric_curve(int type, const float params[], float r)
{
    float e, val, disc;

    switch (type) {

    // X = Y ^ Gamma
    case 1:
        if (r < 0) {

            if (fabs(params[0] - 1.0) < MAX_FLOAT_ERROR)
                val = r;
            else
                val = 0;
        }
        else
            val = pow(r, params[0]);
        break;

        // Type 1 Reversed: X = Y ^1/gamma
    case -1:
        if (r < 0) {

            if (fabs(params[0] - 1.0) < MAX_FLOAT_ERROR)
                val = r;
            else
                val = 0;
        }
        else
            val = pow(r, 1/params[0]);
        break;

        // CIE 122-1966
        // Y = (aX + b)^Gamma  | X >= -b/a
        // Y = 0               | else
    case 2:
        disc = -params[2] / params[1];

        if (r >= disc ) {

            e = params[1]*r + params[2];

            if (e > 0)
                val = pow(e, params[0]);
            else
                val = 0;
        }
        else
            val = 0;
        break;

        // Type 2 Reversed
        // X = (Y ^1/g  - b) / a
    case -2:
        if (r < 0)
            val = 0;
        else
            val = (pow(r, 1.0/params[0]) - params[2]) / params[1];

        if (val < 0)
            val = 0;
        break;


        // IEC 61966-3
        // Y = (aX + b)^Gamma | X <= -b/a
        // Y = c              | else
    case 3:
        disc = -params[2] / params[1];
        if (disc < 0)
            disc = 0;

        if (r >= disc) {

            e = params[1]*r + params[2];

            if (e > 0)
                val = pow(e, params[0]) + params[3];
            else
                val = 0;
        }
        else
            val = params[3];
        break;


        // Type 3 reversed
        // X=((Y-c)^1/g - b)/a      | (Y>=c)
        // X=-b/a                   | (Y<c)
    case -3:
        if (r >= params[3])  {

            e = r - params[3];

            if (e > 0)
                val = (pow(e, 1/params[0]) - params[2]) / params[1];
            else
                val = 0;
        }
        else {
            val = -params[2] / params[1];
        }
        break;


        // IEC 61966-2.1 (sRGB)
            // Y = (aX + b)^Gamma | X >= d
            // Y = cX             | X < d
    case 4:
        if (r >= params[4]) {

            e = params[1]*r + params[2];

            if (e > 0)
                val = pow(e, params[0]);
            else
                val = 0;
        }
        else
            val = r * params[3];
        break;

        // Type 4 reversed
        // X=((Y^1/g-b)/a)    | Y >= (ad+b)^g
        // X=Y/c              | Y< (ad+b)^g
    case -4:
        e = params[1] * params[4] + params[2];
        if (e < 0)
            disc = 0;
        else
            disc = pow(e, params[0]);

        if (r >= disc) {

            val = (pow(r, 1.0/params[0]) - params[2]) / params[1];
        }
        else {
            val = r / params[3];
        }
        break;


        // Y = (aX + b)^Gamma + e | X >= d
                // Y = cX + f             | X < d
    case 5:
        if (r >= params[4]) {

            e = params[1]*r + params[2];

            if (e > 0)
                val = pow(e, params[0]) + params[5];
            else
                val = params[5];
        }
        else
            val = r*params[3] + params[6];
        break;


        // Reversed type 5
        // X=((Y-e)1/g-b)/a   | Y >=(ad+b)^g+e), cd+f
        // X=(Y-f)/c          | else
    case -5:

        disc = params[3] * params[4] + params[6];
        if (r >= disc) {

            e = r - params[5];
            if (e < 0)
                val = 0;
            else
                val = (pow(e, 1.0/params[0]) - params[2]) / params[1];
        }
        else {
            val = (r - params[6]) / params[3];
        }
        break;


        // Types 6,7,8 comes from segmented curves as described in ICCSpecRevision_02_11_06_Float.pdf
        // Type 6 is basically identical to type 5 without d

        // Y = (a * X + b) ^ Gamma + c
    case 6:
        e = params[1]*r + params[2];

        if (e < 0)
            val = params[3];
        else
            val = pow(e, params[0]) + params[3];
        break;

        // ((Y - c) ^1/Gamma - b) / a
    case -6:
        e = r - params[3];
        if (e < 0)
            val = 0;
        else
            val = (pow(e, 1.0/params[0]) - params[2]) / params[1];
        break;


        // Y = a * log (b * X^Gamma + c) + d
    case 7:

        e = params[2] * pow(r, params[0]) + params[3];
        if (e <= 0)
            val = params[4];
        else
            val = params[1]*log10(e) + params[4];
        break;

        // (Y - d) / a = log(b * X ^Gamma + c)
        // pow(10, (Y-d) / a) = b * X ^Gamma + c
        // pow((pow(10, (Y-d) / a) - c) / b, 1/g) = X
    case -7:
        val = pow((pow(10.0, (r-params[4]) / params[1]) - params[3]) / params[2], 1.0 / params[0]);
        break;


        //Y = a * b^(c*X+d) + e
    case 8:
        val = (params[0] * pow(params[1], params[2] * r + params[3]) + params[4]);
        break;


        // Y = (log((y-e) / a) / log(b) - d ) / c
        // a=0, b=1, c=2, d=3, e=4,
    case -8:

        disc = r - params[4];
        if (disc < 0) val = 0;
        else
            val = (log(disc / params[0]) / log(params[1]) - params[3]) / params[2];
        break;

        // S-Shaped: (1 - (1-x)^1/g)^1/g
    case 108:
        val = pow(1.0 - pow(1 - r, 1/params[0]), 1/params[0]);
        break;

        // y = (1 - (1-x)^1/g)^1/g
        // y^g = (1 - (1-x)^1/g)
        // 1 - y^g = (1-x)^1/g
        // (1 - y^g)^g = 1 - x
        // 1 - (1 - y^g)^g
    case -108:
        val = 1 - pow(1 - pow(r, params[0]), params[0]);
        break;

    default:
        // Unsupported parametric curve. Should never reach here
        return 0;
    }

    return val;
}
