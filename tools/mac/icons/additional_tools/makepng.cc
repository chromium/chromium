// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

namespace {

void PNGError(png_struct* png, const char* string) {
  fprintf(stderr, "PNG error: %s\n", string);
}

void PNGWarn(png_struct* png, const char* string) {
  fprintf(stderr, "PNG warning: %s\n", string);
}

uint8_t* ReadFileToBuffer(const char* path, size_t size) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open %s: %s\n", path, strerror(errno));
    return nullptr;
  }

  uint8_t* buf = new uint8_t[size];

  ssize_t read_rv = read(fd, buf, size);
  if (read_rv < 0) {
    fprintf(stderr, "read %s: %s\n", path, strerror(errno));
    delete[] buf;
    close(fd);
    return nullptr;
  } else if (read_rv != size) {
    fprintf(stderr, "read %s: expected %zu, observed %zd (file too small?)\n",
            path, size, read_rv);
    delete[] buf;
    close(fd);
    return nullptr;
  }

  char c;
  read_rv = read(fd, &c, 1);
  if (read_rv < 0) {
    fprintf(stderr, "read %s: %s\n", path, strerror(errno));
    delete[] buf;
    close(fd);
    return nullptr;
  } else if (read_rv != 0) {
    fprintf(stderr, "read %s: expected 0, observed %zd (file too large?)\n",
            path, read_rv);
    delete[] buf;
    close(fd);
    return nullptr;
  }

  if (close(fd) < 0) {
    fprintf(stderr, "close %s: %s\n", path, strerror(errno));
    delete[] buf;
    return nullptr;
  }

  return buf;
}

}  // namespace

bool EncodePNG(const char* input_image_path,
               const char* input_mask_path,
               const char* output_path,
               size_t dimension) {
  const size_t dimension2 = dimension * dimension;

  uint8_t* input_image_buf = ReadFileToBuffer(input_image_path, dimension2 * 3);
  if (!input_image_buf) {
    return false;
  }

  uint8_t* input_mask_buf = ReadFileToBuffer(input_mask_path, dimension2);
  if (!input_mask_buf) {
    delete[] input_image_buf;
    return false;
  }

  uint8_t* merged_buf = new uint8_t[dimension2 * 4];
  std::vector<uint8_t*> rows;
  rows.reserve(dimension);
  for (size_t row = 0; row < dimension; ++row) {
    rows.push_back(&merged_buf[row * dimension * 4]);
    for (size_t col = 0; col < dimension; ++col) {
      const size_t seq = row * dimension + col;

      const uint8_t r = input_image_buf[seq];
      const uint8_t g = input_image_buf[dimension2 + seq];
      const uint8_t b = input_image_buf[dimension2 * 2 + seq];
      const uint8_t a = input_mask_buf[seq];

      merged_buf[seq * 4] = r;
      merged_buf[seq * 4 + 1] = g;
      merged_buf[seq * 4 + 2] = b;
      merged_buf[seq * 4 + 3] = a;
    }
  }

  delete[] input_image_buf;
  delete[] input_mask_buf;

  FILE* output_file = fopen(output_path, "wb");
  if (!output_file) {
    fprintf(stderr, "fopen %s: %s\n", output_path, strerror(errno));
    delete[] merged_buf;
    return false;
  }

  png_struct* png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                            PNGError, PNGWarn);
  if (!png) {
    fprintf(stderr, "png_create_write_struct failed\n");
    fclose(output_file);
    delete[] merged_buf;
    return false;
  }
  png_info* pngi = png_create_info_struct(png);
  if (!pngi) {
    fprintf(stderr, "png_create_info_struct failed\n");
    png_destroy_write_struct(&png, nullptr);
    fclose(output_file);
    delete[] merged_buf;
    return false;
  }

  png_init_io(png, output_file);

  png_set_IHDR(png, pngi, dimension, dimension, 8, PNG_COLOR_TYPE_RGB_ALPHA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, pngi);
  png_write_image(png, &rows[0]);

  png_write_end(png, nullptr);

  delete[] merged_buf;

  if (fclose(output_file) < 0) {
    fprintf(stderr, "fclose %s: %s\n", output_path, strerror(errno));
    return false;
  }

  png_destroy_write_struct(&png, &pngi);

  return true;
}
