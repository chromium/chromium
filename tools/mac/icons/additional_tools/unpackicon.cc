// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://www.macdisk.com/maciconen.php#RLE
// The algorithm is PackBits or PackBits-like.
// Produces a raw RGB image.
// Use with ih32, il32, is32, it32. For it32, use "skip", because there are four
// bytes of unknown use (typically zero) before the compressed data begins.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "unpackicon.h"

bool UnpackIcon(const char* input_path, const char* output_path, bool skip) {
  int input_fd = open(input_path, O_RDONLY);
  if (input_fd < 0) {
    fprintf(stderr, "open %s: %s\n", input_path, strerror(errno));
    return false;
  }

  if (skip) {
    uint32_t skip_buf;
    ssize_t read_skip_rv = read(input_fd, &skip_buf, sizeof(skip_buf));
    if (read_skip_rv < 0) {
      fprintf(stderr, "read %s: %s\n", input_path, strerror(errno));
      return false;
    } else if (read_skip_rv != sizeof(skip_buf)) {
      fprintf(stderr, "read %s: expected %zu, observed %zd\n", input_path,
              sizeof(skip_buf), read_skip_rv);
      return false;
    }
  }

  int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (output_fd < 0) {
    fprintf(stderr, "open %s: %s\n", output_path, strerror(errno));
    return false;
  }

  ssize_t read_cmd_rv;
  do {
    uint8_t cmd;
    read_cmd_rv = read(input_fd, &cmd, 1);
    if (read_cmd_rv < 0) {
      fprintf(stderr, "read %s: %s\n", input_path, strerror(errno));
      return false;
    } else if (read_cmd_rv == 0) {
      break;
    }

    if ((cmd & 0x80) == 0) {
      const size_t count = cmd + 1;
      char buf[128];
      ssize_t read_buf_rv = read(input_fd, buf, count);
      if (read_buf_rv < 0) {
        fprintf(stderr, "read %s: %s\n", input_path, strerror(errno));
        return false;
      } else if (read_buf_rv != count) {
        fprintf(stderr, "read %s: expected %zu, observed %zd\n", input_path,
                count, read_buf_rv);
        return false;
      }

      ssize_t write_buf_rv = write(output_fd, buf, count);
      if (write_buf_rv < 0) {
        fprintf(stderr, "write %s: %s\n", output_path, strerror(errno));
        return false;
      } else if (write_buf_rv != count) {
        fprintf(stderr, "write %s: expected %zu, observed %zd\n", output_path,
                count, write_buf_rv);
        return false;
      }
    } else {
      char rep;
      ssize_t read_rep_rv = read(input_fd, &rep, 1);
      if (read_rep_rv < 0) {
        fprintf(stderr, "read %s: %s\n", input_path, strerror(errno));
        return false;
      }

      const size_t count = cmd - 125;
      std::string reps(count, rep);

      ssize_t write_reps_rv = write(output_fd, &reps[0], reps.size());
      if (write_reps_rv < 0) {
        fprintf(stderr, "write %s: %s\n", output_path, strerror(errno));
        return false;
      } else if (write_reps_rv != reps.size()) {
        fprintf(stderr, "write %s: expected %zu, observed %zd\n", output_path,
                reps.size(), write_reps_rv);
        return false;
      }
    }
  } while (read_cmd_rv > 0);

  if (close(output_fd) < 0) {
    fprintf(stderr, "close %s: %s\n", output_path, strerror(errno));
    return false;
  }

  if (close(input_fd) < 0) {
    fprintf(stderr, "close %s: %s\n", input_path, strerror(errno));
    return false;
  }

  return true;
}
