// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "makepng.h"
#include "unpackicon.h"

#define arraysize(x) (sizeof(x) / sizeof(x[0]))

namespace {

struct IconType {
  uint32_t magic;
  const char* filename;
};

// clang-format off
const IconType icon_types[] = {
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

bool IsPrintableASCIINoSlash(char c) {
  return c >= ' ' && c <= '~' && c != '/';
}

std::string FourCCToASCII(uint32_t fourcc) {
  const char chars[] = {
      fourcc >> 24,
      (fourcc >> 16) & 0xff,
      (fourcc >> 8) & 0xff,
      fourcc & 0xff,
  };

  bool use_ascii = true;
  for (size_t i = 0; i < arraysize(chars); ++i) {
    if (!IsPrintableASCIINoSlash(chars[i])) {
      use_ascii = false;
      break;
    }
  }

  char buf[11];
  if (use_ascii) {
    snprintf(buf, sizeof(buf), "%c%c%c%c", chars[0], chars[1], chars[2],
             chars[3]);
  } else {
    snprintf(buf, sizeof(buf), "0x%x", fourcc);
  }

  return std::string(buf);
}

}  // namespace

int main(int argc, char* argv[]) {
  const char* me = argv[0];

  if (argc != 3) {
    fprintf(stderr, "usage: %s <icns> <iconset>\n", me);
    return EXIT_FAILURE;
  }

  const char* input_path = argv[1];
  int input_fd = open(input_path, O_RDONLY);
  if (input_fd < 0) {
    fprintf(stderr, "%s: open %s: %s\n", me, input_path, strerror(errno));
    return EXIT_FAILURE;
  }

  const char* output_path = argv[2];
  if (mkdir(output_path, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "%s: mkdir %s: %s\n", me, output_path, strerror(errno));
    return EXIT_FAILURE;
  }

  size_t total_read = 0;
  IcnsHeader icns_header;
  ssize_t nread = read(input_fd, &icns_header, sizeof(icns_header));
  if (nread < 0) {
    fprintf(stderr, "%s: read: %s\n", me, strerror(errno));
    return EXIT_FAILURE;
  } else if (nread != sizeof(icns_header)) {
    fprintf(stderr, "%s: read: expected %zu, observed %zd\n", me,
            sizeof(icns_header), nread);
    return EXIT_FAILURE;
  }
  total_read += nread;

  const uint32_t icns_header_magic = ntohl(icns_header.magic);
  if (icns_header_magic != 'icns') {
    fprintf(stderr, "%s: icns file magic: expected 0x%x, observed 0x%x\n", me,
            'icns', icns_header_magic);
    return EXIT_FAILURE;
  }

  const uint32_t icns_header_length = ntohl(icns_header.length);

  struct ImageAndMask {
    std::string image_path;
    std::string mask_path;
    std::string png_path;
  };
  ImageAndMask images_and_masks[4];
  enum ImageAndMaskIndex {
    IMAGE_16,
    IMAGE_32,
    IMAGE_48,
    IMAGE_128,
    IMAGE_NONE,
  };
  const size_t kImageAndMaskDimensions[] = {16, 32, 48, 128};

  while (total_read < icns_header_length) {
    IconHeader icon_header;
    nread = read(input_fd, &icon_header, sizeof(icon_header));
    if (nread < 0) {
      fprintf(stderr, "%s: read: %s\n", me, strerror(errno));
      return EXIT_FAILURE;
    } else if (nread != sizeof(icon_header)) {
      fprintf(stderr, "%s: read: expected %zd, observed %zu\n", me,
              sizeof(icon_header), nread);
      return EXIT_FAILURE;
    }
    total_read += nread;

    size_t icon_length = ntohl(icon_header.length) - sizeof(icon_header);
    char* icon_data = reinterpret_cast<char*>(malloc(icon_length));
    if (!icon_data) {
      fprintf(stderr, "%s: malloc %zu: %s\n", me, icon_length, strerror(errno));
      return EXIT_FAILURE;
    }

    size_t icon_data_read = 0;
    while (icon_length - icon_data_read > 0) {
      nread = read(input_fd, icon_data + icon_data_read,
                   icon_length - icon_data_read);
      if (nread < 0) {
        fprintf(stderr, "%s: read: %s\n", me, strerror(errno));
        return EXIT_FAILURE;
      } else if (nread > icon_length - icon_data_read) {
        fprintf(stderr, "%s: read: expected %zu, observed %zd\n", me,
                icon_length - icon_data_read, nread);
        return EXIT_FAILURE;
      }
      icon_data_read += nread;
    }
    total_read += icon_data_read;

    const uint32_t icon_header_magic = ntohl(icon_header.magic);
    bool found = false;
    std::string output_icon_path = output_path;
    output_icon_path += '/';
    for (size_t icon_type_index = 0; icon_type_index < arraysize(icon_types);
         ++icon_type_index) {
      const IconType& icon_type = icon_types[icon_type_index];
      if (icon_header_magic == icon_type.magic) {
        found = true;
        output_icon_path += "icon_";
        output_icon_path += icon_type.filename;
        output_icon_path += ".png";
        break;
      }
    }

    if (!found) {
      output_icon_path += FourCCToASCII(icon_header_magic);
    }

    const char* output_icon_path_c = output_icon_path.c_str();
    printf("%s\n", output_icon_path_c);

    int output_fd;
    output_fd = open(output_icon_path_c, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
      fprintf(stderr, "%s: open %s: %s\n", me, output_icon_path_c,
              strerror(errno));
      return EXIT_FAILURE;
    }

    size_t icon_data_written = 0;
    while (icon_length - icon_data_written > 0) {
      ssize_t nwritten = write(output_fd, icon_data + icon_data_written,
                               icon_length - icon_data_written);
      if (nwritten < 0) {
        fprintf(stderr, "%s: write: %s\n", me, strerror(errno));
        return EXIT_FAILURE;
      } else if (nwritten > icon_length - icon_data_written) {
        fprintf(stderr, "%s: write: expected %zu, observed %zd\n", me,
                icon_length - icon_data_written, nwritten);
        return EXIT_FAILURE;
      }
      icon_data_written += nwritten;
    }

    if (close(output_fd) < 0) {
      fprintf(stderr, "%s: close %s: %s\n", me, output_icon_path_c,
              strerror(errno));
      return EXIT_FAILURE;
    }

    free(icon_data);

    ImageAndMaskIndex index = IMAGE_NONE;
    switch (icon_header_magic) {
      case 'is32':
        index = IMAGE_16;
        break;
      case 'il32':
        index = IMAGE_32;
        break;
      case 'ih32':
        index = IMAGE_48;
        break;
      case 'it32':
        index = IMAGE_128;
        break;
      case 's8mk':
        images_and_masks[IMAGE_16].mask_path = output_icon_path;
        break;
      case 'l8mk':
        images_and_masks[IMAGE_32].mask_path = output_icon_path;
        break;
      case 'h8mk':
        images_and_masks[IMAGE_48].mask_path = output_icon_path;
        break;
      case 't8mk':
        images_and_masks[IMAGE_128].mask_path = output_icon_path;
        break;
    }

    if (index != IMAGE_NONE) {
      images_and_masks[index].image_path = output_icon_path;
      images_and_masks[index].png_path = output_icon_path + ".png";
    }

    if ((icon_header_magic == 'is32' && icon_length != 16 * 16 * 3) ||
        (icon_header_magic == 'il32' && icon_length != 32 * 32 * 3) ||
        (icon_header_magic == 'ih32' && icon_length != 48 * 48 * 3) ||
        (icon_header_magic == 'it32' && icon_length != 128 * 128 * 3)) {
      const std::string output_icon_unpacked_path =
          output_icon_path + ".unpacked";
      const char* output_icon_unpacked_path_c =
          output_icon_unpacked_path.c_str();
      printf("%s\n", output_icon_unpacked_path_c);

      // is32 and il32 definitely don’t use skip. it32 definitely does. I’m not
      // sure about ih32, but I think it doesn’t use skip.
      const bool skip = icon_header_magic == 'it32';

      if (!UnpackIcon(output_icon_path_c, output_icon_unpacked_path_c, skip)) {
        return EXIT_FAILURE;
      }

      assert(index != IMAGE_NONE);
      images_and_masks[index].image_path = output_icon_unpacked_path;
    }
  }

  for (size_t index = 0; index < arraysize(images_and_masks); ++index) {
    const ImageAndMask& image_and_mask = images_and_masks[index];
    if (!image_and_mask.image_path.empty() &&
        !image_and_mask.mask_path.empty()) {
      printf("%s\n", image_and_mask.png_path.c_str());
      if (!EncodePNG(image_and_mask.image_path.c_str(),
                     image_and_mask.mask_path.c_str(),
                     image_and_mask.png_path.c_str(),
                     kImageAndMaskDimensions[index])) {
        return EXIT_FAILURE;
      }
    }
  }

  if (total_read != icns_header_length) {
    fprintf(stderr, "%s: icns file length: expected %d, observed %zu\n", me,
            icns_header_length, total_read);
    return EXIT_FAILURE;
  }

  if (close(input_fd) < 0) {
    fprintf(stderr, "%s: close %s: %s\n", me, input_path, strerror(errno));
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
