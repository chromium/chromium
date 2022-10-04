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

// Manually update the items below to add more tests.
extern struct qcms_test_case qcms_test_tetra_clut_rgba_info;
extern struct qcms_test_case qcms_test_munsell_info;
extern struct qcms_test_case qcms_test_internal_srgb_info;
extern struct qcms_test_case qcms_test_ntsc_gamut_info;
extern struct qcms_test_case qcms_test_output_trc_info;

struct qcms_test_case qcms_test[5];
#define TEST_CASES    (sizeof(qcms_test) / sizeof(qcms_test[0]))

static void initialize_tests()
{
    qcms_test[0] = qcms_test_tetra_clut_rgba_info;
    qcms_test[1] = qcms_test_munsell_info;
    qcms_test[2] = qcms_test_internal_srgb_info;
    qcms_test[3] = qcms_test_ntsc_gamut_info;
    qcms_test[4] = qcms_test_output_trc_info;
}

static void list_tests()
{
    int i;
    printf("Available qcms tests:\n");

    for (i = 0; i < TEST_CASES; ++i) {
        printf("\t%s\n", qcms_test[i].test_name);
    }

    exit(EXIT_FAILURE);
}

static void print_usage()
{
    printf("Usage:\n\tqcms_test -w WIDTH -h HEIGHT -n ITERATIONS -t TEST\n");
    printf("\t-w INT\t\ttest image width\n");
    printf("\t-h INT\t\ttest image height\n");
    printf("\t-n INT\t\tnumber of iterations for each test\n");
    printf("\t-a\t\trun all tests\n");
    printf("\t-l\t\tlist available tests\n");
    printf("\t-s \t\tforce software(non-sse) transform function, where available\n");
    printf("\t-i STRING\tspecify input icc color profile\n");
    printf("\t-o STRING\tspecify output icc color profile\n");
    printf("\t-t STRING\trun specific test - use \"-l\" to list possible values\n");
    printf("\n");
    exit(1);
}

int enable_test(const char *args)
{
    int i;

    if (!args)
        return 0;
        
    for (i = 0; i < TEST_CASES; ++i) {
        if (strcmp(qcms_test[i].test_name, args) == 0) {
            qcms_test[i].status = QCMS_TEST_ENABLED;
            return 1;
        }
    }

    return 0;
}

int main(int argc, const char **argv)
{
    int iterations = 1;
    size_t height = 2000;
    size_t width = 2000;
    int run_all = 0;
    const char *in = NULL, *out = NULL;
    int force_software = 0;
    int exit_status;
    int enabled_tests = 0;
    int i;

    initialize_tests();
    seconds();

    if (argc == 1) {
        print_usage();
    }

    while (argc > 1) {
        if (strcmp(argv[1], "-n") == 0)
            iterations = abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-w") == 0)
            width = (size_t) abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-h") == 0)
            height = (size_t) abs(atoi(argv[2]));
        else if (strcmp(argv[1], "-l") == 0)
            list_tests();
        else if (strcmp(argv[1], "-t") == 0)
            enabled_tests += enable_test(argv[2]);
        else if (strcmp(argv[1], "-a") == 0)
            run_all = 1;
        else if (strcmp(argv[1], "-i") == 0)
            in = argv[2];
        else if (strcmp(argv[1], "-o") == 0)
            out = argv[2];
        else if (strcmp(argv[1], "-s") == 0)
            force_software = 1;
        (--argc, ++argv);
    }

    if (!run_all && !enabled_tests) {
        print_usage();
    }

    exit_status = 0;

    for (i = 0; i < TEST_CASES; ++i) {
        if (run_all || QCMS_TEST_ENABLED == qcms_test[i].status)
            exit_status += qcms_test[i].test_fn(width, height, iterations, in, out, force_software);
    }

    return exit_status;
}
