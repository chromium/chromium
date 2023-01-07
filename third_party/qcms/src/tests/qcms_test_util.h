// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qcmsint.h"
#include "qcmstypes.h"

typedef int (*qcms_test_function)(size_t width,
        size_t height,
        int iterations,
        const char *in_profile,
        const char *out_profile,
        const int force_software);

enum QCMS_TEST_STATUS {
    QCMS_TEST_DISABLED = 0,
    QCMS_TEST_ENABLED = 1,
};

struct qcms_test_case {
    char test_name[256];
    qcms_test_function test_fn;
    enum QCMS_TEST_STATUS status;
};

void generate_source_uint8_t(unsigned char *src, const size_t length, const size_t pixel_size);
float evaluate_parametric_curve(int type, const float params[], float r);
