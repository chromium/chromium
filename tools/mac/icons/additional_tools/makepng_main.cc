// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <vector>

#include "makepng.h"

int main(int argc, char* argv[]) {
  const char* me = argv[0];
  if (argc != 5) {
    fprintf(stderr, "usage: %s <image> <mask> <png> <dimension>\n", me);
    return EXIT_FAILURE;
  }

  const char* input_image_path = argv[1];
  const char* input_mask_path = argv[2];
  const char* output_png_path = argv[3];
  int dimension = atoi(argv[4]);
  return EncodePNG(input_image_path, input_mask_path, output_png_path,
                   dimension)
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}
