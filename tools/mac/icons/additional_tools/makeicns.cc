// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#define arraysize(x) (sizeof(x) / sizeof(x[0]))

namespace {

struct IconType {
  uint32_t magic;
  const char* filename;
};

// clang-format off
const IconType icon_types[] = {
  { 'is32', nullptr },
  { 's8mk', nullptr },
  { 'il32', nullptr },
  { 'l8mk', nullptr },
  { 'ih32', nullptr },
  { 'h8mk', nullptr },
  { 'it32', nullptr },
  { 't8mk', nullptr },

  { 'icp4', "16x16" },
  { 'icp5', "32x32" },
  { 'icp6', "64x64" },
  { 'ic07', "128x128" },
  { 'ic08', "256x256" },
  { 'ic09', "512x512" },
  { 'ic10', "512x512@2x" },  // previously 1024x1024
  { 'ic11', "16x16@2x" },
  { 'ic12', "32x32@2x" },
  { 'ic13', "128x128@2x" },
  { 'ic14', "256x256@2x" },
};
// clang-format on

struct IcnsHeader {
  uint32_t magic;
  uint32_t length;
};

struct IconHeader {
  uint32_t magic;
  uint32_t length;
};

}  // namespace

int main(int argc, char* argv[]) {
  const char* me = argv[0];

  if (argc != 3) {
    fprintf(stderr, "usage: %s <iconset> <icns>\n", me);
    return EXIT_FAILURE;
  }

  const char* output_path = argv[2];
  int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (output_fd < 0) {
    fprintf(stderr, "%s: open %s: %s\n", me, output_path, strerror(errno));
    return EXIT_FAILURE;
  }

  IcnsHeader icns_header = {};
  if (write(output_fd, &icns_header, sizeof(icns_header)) < 0) {
    fprintf(stderr, "%s: write: %s\n", me, strerror(errno));
    return EXIT_FAILURE;
  }
  icns_header.length += sizeof(icns_header);

  for (size_t icon_type_index = 0; icon_type_index < arraysize(icon_types);
       ++icon_type_index) {
    const IconType& icon_type = icon_types[icon_type_index];
    std::string input_path = std::string(argv[1]) + "/";
    if (icon_type.filename) {
      input_path += std::string("icon_") + icon_type.filename + ".png";
    } else {
      char filename[5];
      snprintf(filename, sizeof(filename), "%c%c%c%c", icon_type.magic >> 24,
               (icon_type.magic >> 16) & 0xff, (icon_type.magic >> 8) & 0xff,
               icon_type.magic & 0xff);
      input_path += filename;
    }
    int input_fd = open(input_path.c_str(), O_RDONLY);
    if (input_fd < 0) {
      if (errno == ENOENT) {
        continue;
      }
      fprintf(stderr, "%s: open %s: %s\n", me, input_path.c_str(),
              strerror(errno));
      return EXIT_FAILURE;
    }

    off_t icon_header_offset = lseek(output_fd, 0, SEEK_CUR);
    if (icon_header_offset < 0) {
      fprintf(stderr, "%s: lseek: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    }

    IconHeader icon_header = {};
    if (write(output_fd, &icon_header, sizeof(icon_header)) < 0) {
      fprintf(stderr, "%s: write: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    }
    icon_header.length = sizeof(icon_header);
    icns_header.length += icon_header.length;

    ssize_t input_read;
    char buf[4096];
    while ((input_read = read(input_fd, buf, sizeof(buf))) > 0) {
      icon_header.length += input_read;
      if (write(output_fd, buf, input_read) < 0) {
        fprintf(stderr, "%s: write: %s\n", me, strerror(errno));
        return EXIT_FAILURE;
      }
      icns_header.length += input_read;
    }
    if (input_read < 0) {
      fprintf(stderr, "%s: read %s: %s\n", me, input_path.c_str(),
              strerror(errno));
      return EXIT_FAILURE;
    }

    if (close(input_fd) < 0) {
      fprintf(stderr, "%s: close %s: %s\n", me, input_path.c_str(),
              strerror(errno));
      return EXIT_FAILURE;
    }

    off_t end_offset = lseek(output_fd, 0, SEEK_CUR);
    if (end_offset < 0) {
      fprintf(stderr, "%s: lseek: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    }

    if (lseek(output_fd, icon_header_offset, SEEK_SET) < 0) {
      fprintf(stderr, "%s: lseek: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    }

    icon_header.magic = htonl(icon_type.magic);
    icon_header.length = htonl(icon_header.length);
    if (write(output_fd, &icon_header, sizeof(icon_header)) < 0) {
      fprintf(stderr, "%s: write: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    }

    if (lseek(output_fd, end_offset, SEEK_SET) < 0) {
      fprintf(stderr, "%s: lseek: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    }
  }

  if (lseek(output_fd, 0, SEEK_SET) < 0) {
    fprintf(stderr, "%s: lseek: %s\n", me, strerror(errno));
    return EXIT_FAILURE;
  }

  icns_header.magic = htonl('icns');
  icns_header.length = htonl(icns_header.length);

  if (write(output_fd, &icns_header, sizeof(icns_header)) < 0) {
    fprintf(stderr, "%s: write: %s\n", me, strerror(errno));
    return EXIT_FAILURE;
  }

  if (close(output_fd) < 0) {
    fprintf(stderr, "%s: close %s: %s\n", me, output_path, strerror(errno));
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
