// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "unpackicon.h"

int main(int argc, char* argv[]) {
  const char* me = argv[0];

  bool skip = false;
  if (argc == 4 && strcmp(argv[1], "-s") == 0) {
    skip = true;
    --argc;
    argv[1] = argv[2];
    argv[2] = argv[3];
  }

  if (argc != 3) {
    fprintf(stderr, "usage: %s [-s] <packed> <unpacked>\n", me);
    return EXIT_FAILURE;
  }

  const char* input_path = argv[1];
  const char* output_path = argv[2];

  return UnpackIcon(input_path, output_path, skip) ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;
}
